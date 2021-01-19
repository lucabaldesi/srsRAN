/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include <bitset>
#include <inttypes.h>
#include <iostream>
#include <string.h>

#include "srsenb/hdr/stack/mac/ue.h"
#include "srslte/common/log_helper.h"
#include "srslte/interfaces/enb_interfaces.h"

namespace srsenb {

ue::ue(uint16_t                 rnti_,
       uint32_t                 nof_prb_,
       sched_interface*         sched_,
       rrc_interface_mac*       rrc_,
       rlc_interface_mac*       rlc_,
       phy_interface_stack_lte* phy_,
       srslte::log_ref          log_,
       uint32_t                 nof_cells_,
       uint32_t                 nof_rx_harq_proc_,
       uint32_t                 nof_tx_harq_proc_) :
  rnti(rnti_),
  nof_prb(nof_prb_),
  sched(sched_),
  rrc(rrc_),
  rlc(rlc_),
  phy(phy_),
  log_h(log_),
  mac_msg_dl(20, log_),
  mch_mac_msg_dl(10, log_),
  mac_msg_ul(20, log_),
  pdus(128),
  nof_rx_harq_proc(nof_rx_harq_proc_),
  nof_tx_harq_proc(nof_tx_harq_proc_),
  ta_fsm(this)
{
  srslte::byte_buffer_pool* pool = srslte::byte_buffer_pool::get_instance();
  tx_payload_buffer.resize(nof_cells_);
  for (auto& carrier_buffers : tx_payload_buffer) {
    for (auto& harq_buffers : carrier_buffers) {
      for (srslte::unique_byte_buffer_t& tb_buffer : harq_buffers) {
        tb_buffer = srslte::allocate_unique_buffer(*pool);
      }
    }
  }

  pdus.init(this, log_h);

  // Allocate buffer for PCell
  allocate_cc_buffers();
}

ue::~ue()
{
  // Free up all softbuffers for all CCs
  for (auto cc : softbuffer_rx) {
    for (auto buffer : cc) {
      srslte_softbuffer_rx_free(&buffer);
    }
  }

  for (auto cc : softbuffer_tx) {
    for (auto buffer : cc) {
      srslte_softbuffer_tx_free(&buffer);
    }
  }
}

void ue::reset()
{
  ue_metrics   = {};
  nof_failures = 0;

  for (auto cc : softbuffer_rx) {
    for (auto buffer : cc) {
      srslte_softbuffer_rx_reset(&buffer);
    }
  }

  for (auto cc : softbuffer_tx) {
    for (auto buffer : cc) {
      srslte_softbuffer_tx_reset(&buffer);
    }
  }
}

/**
 * Allocate and initialize softbuffers for Tx and Rx and
 * append to current list of CC buffers. It uses the configured
 * number of HARQ processes and cell width.
 *
 * @param num_cc Number of carriers to add buffers for (default 1)
 * @return number of carriers
 */
uint32_t ue::allocate_cc_buffers(const uint32_t num_cc)
{
  for (uint32_t i = 0; i < num_cc; ++i) {
    // create and init Rx buffers for Pcell
    softbuffer_rx.emplace_back();
    softbuffer_rx.back().resize(nof_rx_harq_proc);
    for (auto& buffer : softbuffer_rx.back()) {
      srslte_softbuffer_rx_init(&buffer, nof_prb);
    }

    // Create and init Tx buffers for Pcell
    softbuffer_tx.emplace_back();
    softbuffer_tx.back().resize(nof_tx_harq_proc);
    for (auto& buffer : softbuffer_tx.back()) {
      srslte_softbuffer_tx_init(&buffer, nof_prb);
    }
    // don't need to reset because just initiated the buffers
  }
  return softbuffer_tx.size();
}

void ue::start_pcap(srslte::mac_pcap* pcap_)
{
  pcap = pcap_;
}

srslte_softbuffer_rx_t* ue::get_rx_softbuffer(const uint32_t ue_cc_idx, const uint32_t tti)
{
  if ((size_t)ue_cc_idx >= softbuffer_rx.size()) {
    ERROR("UE CC Index (%d/%zd) out-of-range\n", ue_cc_idx, softbuffer_rx.size());
    return nullptr;
  }

  if ((size_t)nof_rx_harq_proc > softbuffer_rx.at(ue_cc_idx).size()) {
    ERROR("HARQ process index (%d/%zd) out-of-range\n", nof_rx_harq_proc, softbuffer_rx.at(ue_cc_idx).size());
    return nullptr;
  }

  return &softbuffer_rx.at(ue_cc_idx).at(tti % nof_rx_harq_proc);
}

srslte_softbuffer_tx_t*
ue::get_tx_softbuffer(const uint32_t ue_cc_idx, const uint32_t harq_process, const uint32_t tb_idx)
{
  if ((size_t)ue_cc_idx >= softbuffer_tx.size()) {
    ERROR("UE CC Index (%d/%zd) out-of-range\n", ue_cc_idx, softbuffer_tx.size());
    return nullptr;
  }

  if ((size_t)nof_tx_harq_proc > softbuffer_tx.at(ue_cc_idx).size()) {
    ERROR("HARQ process index (%d/%zd) out-of-range\n", harq_process, softbuffer_tx.at(ue_cc_idx).size());
    return nullptr;
  }

  return &softbuffer_tx.at(ue_cc_idx).at((harq_process * SRSLTE_MAX_TB + tb_idx) % nof_tx_harq_proc);
}

uint8_t* ue::request_buffer(uint32_t tti, const uint32_t len)
{
  uint8_t* pdu = nullptr;
  if (len > 0) {
    pdu = pdus.request(len);
    if (pdu) {
      // Deallocate oldest buffer if we didn't deallocate it
      if (rx_used_buffers[tti] != nullptr) {
        pdus.deallocate(rx_used_buffers[tti]);
        rx_used_buffers[tti] = nullptr;
        log_h->warning("buffers: RX PDU of rnti=0x%x and pid=%d wasn't deallocated\n", rnti, tti % nof_rx_harq_proc);
      }
      rx_used_buffers[tti] = pdu;
      log_h->info("RX PDU saved for pid=%d\n", tti % nof_rx_harq_proc);
    } else {
      log_h->error("buffers: Requesting buffer from pool\n");
    }
  } else {
    printf("buffers: Requesting buffer for zero bytes\n");
  }
  return pdu;
}

bool ue::process_pdus()
{
  return pdus.process_pdus();
}

void ue::set_tti(uint32_t tti)
{
  last_tti = tti;
}

uint32_t ue::set_ta(int ta_)
{
  int      ta       = ta_;
  uint32_t nof_cmd  = 0;
  int      ta_value = 0;
  do {
    ta_value = SRSLTE_MAX(-31, SRSLTE_MIN(32, ta));
    ta -= ta_value;
    uint32_t ta_cmd = (uint32_t)(ta_value + 31);
    pending_ta_commands.try_push(ta_cmd);
    nof_cmd++;
    Info("Added TA CMD: rnti=0x%x, ta=%d, ta_value=%d, ta_cmd=%d\n", rnti, ta_, ta_value, ta_cmd);
  } while (ta_value <= -31 || ta_value >= 32);
  return nof_cmd;
}

#include <assert.h>

void ue::process_pdu(uint8_t* pdu, uint32_t nof_bytes, srslte::pdu_queue::channel_t channel)
{
  // Unpack ULSCH MAC PDU
  mac_msg_ul.init_rx(nof_bytes, true);
  mac_msg_ul.parse_packet(pdu);

  Info("0x%x %s\n", rnti, mac_msg_ul.to_string().c_str());

  if (pcap) {
    pcap->write_ul_crnti(pdu, nof_bytes, rnti, true, last_tti, UL_CC_IDX);
  }

  pdus.deallocate(pdu);

  uint32_t lcid_most_data = 0;
  int      most_data      = -99;

  while (mac_msg_ul.next()) {
    assert(mac_msg_ul.get());
    if (mac_msg_ul.get()->is_sdu()) {
      /* In some cases, an uplink transmission with only CQI has all zeros and gets routed to RRC
       * Compute the checksum if lcid=0 and avoid routing in that case
       */
      bool route_pdu = true;
      if (mac_msg_ul.get()->get_sdu_lcid() == 0) {
        uint8_t* x   = mac_msg_ul.get()->get_sdu_ptr();
        uint32_t sum = 0;
        for (uint32_t i = 0; i < mac_msg_ul.get()->get_payload_size(); i++) {
          sum += x[i];
        }
        if (sum == 0) {
          route_pdu = false;
          Debug("Received all zero PDU\n");
        }
      }

      if (route_pdu) {
        rlc->write_pdu(rnti,
                       mac_msg_ul.get()->get_sdu_lcid(),
                       mac_msg_ul.get()->get_sdu_ptr(),
                       mac_msg_ul.get()->get_payload_size());
      }

      // Indicate scheduler to update BSR counters
      // sched->ul_recv_len(rnti, mac_msg_ul.get()->get_sdu_lcid(), mac_msg_ul.get()->get_payload_size());

      // Indicate RRC about successful activity if valid RLC message is received
      if (mac_msg_ul.get()->get_payload_size() > 64) { // do not count RLC status messages only
        rrc->set_activity_user(rnti);
        log_h->debug("UL activity rnti=0x%x, n_bytes=%d\n", rnti, nof_bytes);
      }

      if ((int)mac_msg_ul.get()->get_payload_size() > most_data) {
        most_data      = (int)mac_msg_ul.get()->get_payload_size();
        lcid_most_data = mac_msg_ul.get()->get_sdu_lcid();
      }

      // Save contention resolution if lcid == 0
      if (mac_msg_ul.get()->get_sdu_lcid() == 0 && route_pdu) {
        int nbytes = srslte::sch_subh::MAC_CE_CONTRES_LEN;
        if (mac_msg_ul.get()->get_payload_size() >= (uint32_t)nbytes) {
          uint8_t* ue_cri_ptr = (uint8_t*)&conres_id;
          uint8_t* pkt_ptr    = mac_msg_ul.get()->get_sdu_ptr(); // Warning here: we want to include the
          for (int i = 0; i < nbytes; i++) {
            ue_cri_ptr[nbytes - i - 1] = pkt_ptr[i];
          }
        } else {
          Error("Received CCCH UL message of invalid size=%d bytes\n", mac_msg_ul.get()->get_payload_size());
        }
      }
    }
  }
  mac_msg_ul.reset();

  /* Process CE after all SDUs because we need to update BSR after */
  bool bsr_received = false;
  while (mac_msg_ul.next()) {
    assert(mac_msg_ul.get());
    if (!mac_msg_ul.get()->is_sdu()) {
      // Process MAC Control Element
      bsr_received |= process_ce(mac_msg_ul.get());
    }
  }

  // If BSR is not received means that new data has arrived and there is no space for BSR transmission
  if (!bsr_received && lcid_most_data > 2) {
    // Add BSR to the LCID for which most data was received
    sched->ul_buffer_add(rnti, lcid_most_data, 256);
    Debug("BSR not received. Giving extra dci\n");
  }

  Debug("MAC PDU processed\n");
}

void ue::deallocate_pdu(uint32_t tti, const uint8_t* pdu_ptr)
{
  if (pdu_ptr) {
    if (rx_used_buffers[tti] == pdu_ptr) {
      rx_used_buffers[tti] = nullptr;
    } else {
      Warning("buffers: Unexpected RX PDU pointer in deallocate_pdu for rnti=0x%x pid=%d\n", rnti, tti % nof_rx_harq_proc);
    }
    pdus.deallocate(pdu_ptr);
  } else {
    Error("Error deallocating PDU: null ptr\n");
  }
}

void ue::push_pdu(uint32_t tti, const uint8_t* pdu_ptr, uint32_t len)
{
  if (pdu_ptr && len > 0) {
    if (rx_used_buffers[tti] == pdu_ptr) {
      rx_used_buffers[tti] = nullptr;
    } else {
      Warning("buffers: Unexpected RX PDU pointer in push_pdu for rnti=0x%x pid=%d\n", rnti, tti % nof_rx_harq_proc);
    }
    pdus.push(pdu_ptr, len);
  } else {
    Error("Error pushing PDU: ptr=%p, len=%d\n", pdu_ptr, len);
  }
}

bool ue::process_ce(srslte::sch_subh* subh)
{
  uint32_t buff_size_idx[4]   = {};
  uint32_t buff_size_bytes[4] = {};
  float    phr                = 0;
  int32_t  idx                = 0;
  uint16_t old_rnti           = 0;
  bool     is_bsr             = false;
  switch (subh->ul_sch_ce_type()) {
    case srslte::ul_sch_lcid::PHR_REPORT:
      phr = subh->get_phr();
      sched->ul_phr(rnti, (int)phr);
      metrics_phr(phr);
      break;
    case srslte::ul_sch_lcid::CRNTI:
      old_rnti = subh->get_c_rnti();
      if (sched->ue_exists(old_rnti)) {
        rrc->upd_user(rnti, old_rnti);
        rnti = old_rnti;
      } else {
        Error("Updating user C-RNTI: rnti=0x%x already released\n", old_rnti);
      }
      break;
    case srslte::ul_sch_lcid::TRUNC_BSR:
    case srslte::ul_sch_lcid::SHORT_BSR:
      idx = subh->get_bsr(buff_size_idx, buff_size_bytes);
      if (idx == -1) {
        Error("Invalid Index Passed to lc groups\n");
        break;
      }
      // Indicate BSR to scheduler
      sched->ul_bsr(rnti, idx, buff_size_bytes[idx]);
      is_bsr = true;
      break;
    case srslte::ul_sch_lcid::LONG_BSR:
      subh->get_bsr(buff_size_idx, buff_size_bytes);
      for (idx = 0; idx < sched_interface::MAX_LC_GROUP; ++idx) {
        sched->ul_bsr(rnti, idx, buff_size_bytes[idx]);
      }
      is_bsr = true;
      break;
    case srslte::ul_sch_lcid::PADDING:
      break;
    default:
      Error("CE:    Invalid lcid=0x%x\n", (int)subh->ul_sch_ce_type());
      break;
  }
  return is_bsr;
}

int ue::read_pdu(uint32_t lcid, uint8_t* payload, uint32_t requested_bytes)
{
  return rlc->read_pdu(rnti, lcid, payload, requested_bytes);
}

void ue::allocate_sdu(srslte::sch_pdu* pdu, uint32_t lcid, uint32_t total_sdu_len)
{
  const int min_sdu_len = lcid == 0 ? 1 : 2;
  int       sdu_space   = pdu->get_sdu_space();
  if (sdu_space > 0) {
    int sdu_len = SRSLTE_MIN(total_sdu_len, (uint32_t)sdu_space);
    int n       = 1;
    while (sdu_len >= min_sdu_len && n > 0) { // minimum size is a single RLC AM status PDU (2 Byte)
      if (pdu->new_subh()) {                  // there is space for a new subheader
        log_h->debug("SDU:   set_sdu(), lcid=%d, sdu_len=%d, sdu_space=%d\n", lcid, sdu_len, sdu_space);
        n = pdu->get()->set_sdu(lcid, sdu_len, this);
        if (n > 0) { // new SDU could be added
          sdu_len -= n;
          log_h->debug("SDU:   rnti=0x%x, lcid=%d, nbytes=%d, rem_len=%d\n", rnti, lcid, n, sdu_len);
        } else {
          Debug("Could not add SDU lcid=%d nbytes=%d, space=%d\n", lcid, sdu_len, sdu_space);
          pdu->del_subh();
        }
      } else {
        n = 0;
      }
    }
  }
}

void ue::allocate_ce(srslte::sch_pdu* pdu, uint32_t lcid)
{
  switch ((srslte::dl_sch_lcid)lcid) {
    case srslte::dl_sch_lcid::TA_CMD:
      if (pdu->new_subh()) {
        uint32_t ta_cmd = 31;
        pending_ta_commands.try_pop(&ta_cmd);
        if (!pdu->get()->set_ta_cmd(ta_cmd)) {
          Error("CE:    Setting TA CMD CE\n");
        }
      } else {
        Error("CE:    Setting TA CMD CE. No space for a subheader\n");
      }
      break;
    case srslte::dl_sch_lcid::CON_RES_ID:
      if (pdu->new_subh()) {
        if (!pdu->get()->set_con_res_id(conres_id)) {
          Error("CE:    Setting Contention Resolution ID CE\n");
        }
      } else {
        Error("CE:    Setting Contention Resolution ID CE. No space for a subheader\n");
      }
      break;
    case srslte::dl_sch_lcid::SCELL_ACTIVATION:
      if (pdu->new_subh()) {
        std::array<bool, SRSLTE_MAX_CARRIERS> active_scell_list = sched->get_scell_activation_mask(rnti);
        if (pdu->get()->set_scell_activation_cmd(active_scell_list)) {
          phy->set_activation_deactivation_scell(rnti, active_scell_list);
          // Allocate and initialize Rx/Tx softbuffers for new carriers (exclude PCell)
          allocate_cc_buffers(active_scell_list.size() - 1);
        } else {
          Error("CE:    Setting SCell Activation CE\n");
        }
      } else {
        Error("CE:    Setting SCell Activation CE. No space for a subheader\n");
      }
      break;
    default:
      Error("CE:    Allocating CE=0x%x. Not supported\n", lcid);
      break;
  }
}

uint8_t* ue::generate_pdu(uint32_t                        ue_cc_idx,
                          uint32_t                        harq_pid,
                          uint32_t                        tb_idx,
                          sched_interface::dl_sched_pdu_t pdu[sched_interface::MAX_RLC_PDU_LIST],
                          uint32_t                        nof_pdu_elems,
                          uint32_t                        grant_size)
{
  std::lock_guard<std::mutex> lock(mutex);
  uint8_t*                    ret = nullptr;
  if (rlc) {
    if (ue_cc_idx < SRSLTE_MAX_CARRIERS && harq_pid < SRSLTE_FDD_NOF_HARQ && tb_idx < SRSLTE_MAX_TB) {
      tx_payload_buffer[ue_cc_idx][harq_pid][tb_idx]->clear();
      mac_msg_dl.init_tx(tx_payload_buffer[ue_cc_idx][harq_pid][tb_idx].get(), grant_size, false);
      for (uint32_t i = 0; i < nof_pdu_elems; i++) {
        if (pdu[i].lcid <= (uint32_t)srslte::ul_sch_lcid::PHR_REPORT) {
          allocate_sdu(&mac_msg_dl, pdu[i].lcid, pdu[i].nbytes);
        } else {
          allocate_ce(&mac_msg_dl, pdu[i].lcid);
        }
      }
      ret = mac_msg_dl.write_packet(log_h);
      Info("0x%x %s\n", rnti, mac_msg_dl.to_string().c_str());
    } else {
      log_h->error(
          "Invalid parameters calling generate_pdu: cc_idx=%d, harq_pid=%d, tb_idx=%d\n", ue_cc_idx, harq_pid, tb_idx);
    }
  } else {
    std::cout << "Error ue not configured (must call config() first" << std::endl;
  }
  return ret;
}

uint8_t* ue::generate_mch_pdu(uint32_t                      harq_pid,
                              sched_interface::dl_pdu_mch_t sched,
                              uint32_t                      nof_pdu_elems,
                              uint32_t                      grant_size)
{
  std::lock_guard<std::mutex> lock(mutex);
  uint8_t*                    ret = nullptr;
  tx_payload_buffer[0][harq_pid][0]->clear();
  mch_mac_msg_dl.init_tx(tx_payload_buffer[0][harq_pid][0].get(), grant_size);

  for (uint32_t i = 0; i < nof_pdu_elems; i++) {
    if (sched.pdu[i].lcid == (uint32_t)srslte::mch_lcid::MCH_SCHED_INFO) {
      mch_mac_msg_dl.new_subh();
      mch_mac_msg_dl.get()->set_next_mch_sched_info(sched.mtch_sched[i].lcid, sched.mtch_sched[i].stop);
    } else if (sched.pdu[i].lcid == 0) {
      mch_mac_msg_dl.new_subh();
      mch_mac_msg_dl.get()->set_sdu(0, sched.pdu[i].nbytes, sched.mcch_payload);
    } else if (sched.pdu[i].lcid <= (uint32_t)srslte::mch_lcid::MTCH_MAX_LCID) {
      mch_mac_msg_dl.new_subh();
      mch_mac_msg_dl.get()->set_sdu(sched.pdu[i].lcid, sched.pdu[i].nbytes, sched.mtch_sched[i].mtch_payload);
    }
  }

  ret = mch_mac_msg_dl.write_packet(log_h);
  return ret;
}

/******* METRICS interface ***************/
void ue::metrics_read(mac_ue_metrics_t* metrics_)
{
  ue_metrics.rnti      = rnti;
  ue_metrics.ul_buffer = sched->get_ul_buffer(rnti);
  ue_metrics.dl_buffer = sched->get_dl_buffer(rnti);

  // set PCell sector id
  std::array<int, SRSLTE_MAX_CARRIERS> cc_list = sched->get_enb_ue_cc_map(rnti);
  auto                                 it      = std::find(cc_list.begin(), cc_list.end(), 0);
  ue_metrics.cc_idx                            = std::distance(cc_list.begin(), it);

  *metrics_ = ue_metrics;

  phr_counter    = 0;
  dl_cqi_counter = 0;
  ue_metrics     = {};
}

void ue::metrics_phr(float phr)
{
  ue_metrics.phr = SRSLTE_VEC_CMA(phr, ue_metrics.phr, phr_counter);
  phr_counter++;
}

void ue::metrics_dl_ri(uint32_t dl_ri)
{
  if (ue_metrics.dl_ri == 0.0f) {
    ue_metrics.dl_ri = (float)dl_ri + 1.0f;
  } else {
    ue_metrics.dl_ri = SRSLTE_VEC_EMA((float)dl_ri + 1.0f, ue_metrics.dl_ri, 0.5f);
  }
  dl_ri_counter++;
}

void ue::metrics_dl_pmi(uint32_t dl_ri)
{
  ue_metrics.dl_pmi = SRSLTE_VEC_CMA((float)dl_ri, ue_metrics.dl_pmi, dl_pmi_counter);
  dl_pmi_counter++;
}

void ue::metrics_dl_cqi(uint32_t dl_cqi)
{
  ue_metrics.dl_cqi = SRSLTE_VEC_CMA((float)dl_cqi, ue_metrics.dl_cqi, dl_cqi_counter);
  dl_cqi_counter++;
}

void ue::metrics_rx(bool crc, uint32_t tbs)
{
  if (crc) {
    ue_metrics.rx_brate += tbs * 8;
  } else {
    ue_metrics.rx_errors++;
  }
  ue_metrics.rx_pkts++;
}

void ue::metrics_tx(bool crc, uint32_t tbs)
{
  if (crc) {
    ue_metrics.tx_brate += tbs * 8;
  } else {
    ue_metrics.tx_errors++;
  }
  ue_metrics.tx_pkts++;
}

void ue::metrics_cnt()
{
  ue_metrics.nof_tti++;
}

void ue::tic()
{
  // Check for pending TA commands
  uint32_t nof_ta_count = ta_fsm.tick();
  if (nof_ta_count) {
    sched->dl_mac_buffer_state(rnti, (uint32_t)srslte::dl_sch_lcid::TA_CMD, nof_ta_count);
  }
}

} // namespace srsenb
