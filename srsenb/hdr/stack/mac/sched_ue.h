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

#ifndef SRSENB_SCHEDULER_UE_H
#define SRSENB_SCHEDULER_UE_H

#include "sched_common.h"
#include "srslte/common/log.h"
#include <map>
#include <vector>

#include "sched_harq.h"
#include "sched_lch.h"
#include <bitset>
#include <deque>

namespace srsenb {

typedef enum { UCI_PUSCH_NONE = 0, UCI_PUSCH_CQI, UCI_PUSCH_ACK, UCI_PUSCH_ACK_CQI } uci_pusch_t;
enum class cc_st { active, idle, activating, deactivating };

struct cc_sched_ue {
  const static int SCHED_MAX_HARQ_PROC = FDD_HARQ_DELAY_UL_MS + FDD_HARQ_DELAY_DL_MS;

  cc_sched_ue(const sched_interface::ue_cfg_t& cfg_,
              const sched_cell_params_t&       cell_cfg_,
              uint16_t                         rnti_,
              uint32_t                         ue_cc_idx,
              srslte::tti_point                current_tti);
  void reset();
  void set_cfg(const sched_interface::ue_cfg_t& cfg); ///< reconfigure ue carrier
  void finish_tti(srslte::tti_point tti_rx);

  uint32_t                   get_aggr_level(uint32_t nof_bits);
  int                        alloc_tbs(uint32_t nof_prb, uint32_t nof_re, uint32_t req_bytes, bool is_ul, int* mcs);
  int                        alloc_tbs_dl(uint32_t nof_prb, uint32_t nof_re, uint32_t req_bytes, int* mcs);
  int                        alloc_tbs_ul(uint32_t nof_prb, uint32_t nof_re, uint32_t req_bytes, int* mcs);
  int                        get_required_prb_dl(uint32_t req_bytes, uint32_t nof_ctrl_symbols);
  uint32_t                   get_required_prb_ul(uint32_t req_bytes);
  const sched_cell_params_t* get_cell_cfg() const { return cell_params; }
  uint32_t                   get_ue_cc_idx() const { return ue_cc_idx; }
  void                       set_dl_cqi(uint32_t tti_tx_dl, uint32_t dl_cqi);
  int                        cqi_to_tbs(uint32_t nof_prb, uint32_t nof_re, bool is_ul, uint32_t* mcs);
  cc_st                      cc_state() const { return cc_state_; }

  harq_entity harq_ent;

  uint32_t dl_ri      = 0;
  uint32_t dl_ri_tti  = 0;
  uint32_t dl_pmi     = 0;
  uint32_t dl_pmi_tti = 0;
  uint32_t dl_cqi     = 1;
  uint32_t dl_cqi_tti = 0;
  uint32_t ul_cqi     = 1;
  uint32_t ul_cqi_tti = 0;
  bool     dl_cqi_rx  = false;

  uint32_t max_mcs_dl = 28, max_mcs_ul = 28;
  uint32_t max_aggr_level = 3;
  int      fixed_mcs_ul = 0, fixed_mcs_dl = 0;

  // Allowed DCI locations per per CFI and per subframe
  std::array<std::array<sched_dci_cce_t, 10>, 3> dci_locations = {};

private:
  // config
  srslte::log_ref                  log_h;
  const sched_interface::ue_cfg_t* cfg         = nullptr;
  const sched_cell_params_t*       cell_params = nullptr;
  uint16_t                         rnti;
  uint32_t                         ue_cc_idx = 0;
  srslte::tti_point                cfg_tti;

  // state
  srslte::tti_point last_tti;
  cc_st             cc_state_ = cc_st::idle;
};

const char* to_string(sched_interface::ue_bearer_cfg_t::direction_t dir);

/** This class is designed to be thread-safe because it is called from workers through scheduler thread and from
 * higher layers and mac threads.
 */
class sched_ue
{
public:
  sched_ue();
  void reset();
  void init(uint16_t rnti, const std::vector<sched_cell_params_t>& cell_list_params_);
  void new_tti(srslte::tti_point new_tti);

  /*************************************************************
   *
   * FAPI-like Interface
   *
   ************************************************************/
  void phy_config_enabled(uint32_t tti, bool enabled);
  void set_cfg(const sched_interface::ue_cfg_t& cfg);

  void set_bearer_cfg(uint32_t lc_id, srsenb::sched_interface::ue_bearer_cfg_t* cfg);
  void rem_bearer(uint32_t lc_id);

  void dl_buffer_state(uint8_t lc_id, uint32_t tx_queue, uint32_t retx_queue);
  void ul_buffer_state(uint8_t lcg_id, uint32_t bsr);
  void ul_phr(int phr);
  void mac_buffer_state(uint32_t ce_code, uint32_t nof_cmds);

  void set_ul_cqi(uint32_t tti, uint32_t enb_cc_idx, uint32_t cqi, uint32_t ul_ch_code);
  void set_dl_ri(uint32_t tti, uint32_t enb_cc_idx, uint32_t ri);
  void set_dl_pmi(uint32_t tti, uint32_t enb_cc_idx, uint32_t ri);
  void set_dl_cqi(uint32_t tti, uint32_t enb_cc_idx, uint32_t cqi);
  int  set_ack_info(uint32_t tti, uint32_t enb_cc_idx, uint32_t tb_idx, bool ack);
  void set_ul_crc(srslte::tti_point tti_rx, uint32_t enb_cc_idx, bool crc_res);

  /*******************************************************
   * Custom functions
   *******************************************************/

  void tpc_inc();
  void tpc_dec();

  const dl_harq_proc&              get_dl_harq(uint32_t idx, uint32_t cc_idx) const;
  uint16_t                         get_rnti() const { return rnti; }
  std::pair<bool, uint32_t>        get_active_cell_index(uint32_t enb_cc_idx) const;
  const sched_interface::ue_cfg_t& get_ue_cfg() const { return cfg; }
  uint32_t                         get_aggr_level(uint32_t ue_cc_idx, uint32_t nof_bits);
  void                             ul_buffer_add(uint8_t lcid, uint32_t bytes);

  /*******************************************************
   * Functions used by scheduler metric objects
   *******************************************************/

  uint32_t get_required_prb_ul(uint32_t cc_idx, uint32_t req_bytes);

  rbg_interval               get_required_dl_rbgs(uint32_t ue_cc_idx);
  srslte::interval<uint32_t> get_requested_dl_bytes(uint32_t ue_cc_idx);
  uint32_t                   get_pending_dl_rlc_data() const;
  uint32_t                   get_expected_dl_bitrate(uint32_t ue_cc_idx) const;

  uint32_t get_pending_ul_data_total(uint32_t tti, int this_ue_cc_idx);
  uint32_t get_pending_ul_new_data(uint32_t tti, int this_ue_cc_idx);
  uint32_t get_pending_ul_old_data();
  uint32_t get_pending_ul_old_data(uint32_t cc_idx);
  uint32_t get_expected_ul_bitrate(uint32_t ue_cc_idx) const;

  dl_harq_proc* get_pending_dl_harq(uint32_t tti_tx_dl, uint32_t cc_idx);
  dl_harq_proc* get_empty_dl_harq(uint32_t tti_tx_dl, uint32_t cc_idx);
  ul_harq_proc* get_ul_harq(uint32_t tti, uint32_t ue_cc_idx);

  /*******************************************************
   * Functions used by the scheduler carrier object
   *******************************************************/

  void finish_tti(const tti_params_t& tti_params, uint32_t enb_cc_idx);

  /*******************************************************
   * Functions used by the scheduler object
   *******************************************************/

  void set_sr();
  void unset_sr();

  int generate_dl_dci_format(uint32_t                          pid,
                             sched_interface::dl_sched_data_t* data,
                             uint32_t                          tti,
                             uint32_t                          ue_cc_idx,
                             uint32_t                          cfi,
                             const rbgmask_t&                  user_mask);
  int generate_format0(sched_interface::ul_sched_data_t* data,
                       uint32_t                          tti,
                       uint32_t                          cc_idx,
                       prb_interval                      alloc,
                       bool                              needs_pdcch,
                       srslte_dci_location_t             cce_range,
                       int                               explicit_mcs = -1,
                       uci_pusch_t                       uci_type     = UCI_PUSCH_NONE);

  srslte_dci_format_t get_dci_format();
  sched_dci_cce_t*    get_locations(uint32_t enb_cc_idx, uint32_t current_cfi, uint32_t sf_idx);

  cc_sched_ue*                     find_ue_carrier(uint32_t enb_cc_idx);
  size_t                           nof_carriers_configured() const { return carriers.size(); }
  std::bitset<SRSLTE_MAX_CARRIERS> scell_activation_mask() const;
  int                              enb_to_ue_cc_idx(uint32_t enb_cc_idx) const;

  bool     needs_cqi(uint32_t tti, uint32_t cc_idx, bool will_send = false);
  uint32_t get_max_retx();

  bool pucch_sr_collision(uint32_t tti, uint32_t n_cce);
  bool pdsch_enabled(srslte::tti_point tti_rx, uint32_t enb_cc_idx) const;
  bool pusch_enabled(srslte::tti_point tti_rx, uint32_t enb_cc_idx, bool needs_pdcch) const;

private:
  void check_ue_cfg_correctness() const;
  bool is_sr_triggered();

  std::pair<int, int> allocate_new_dl_mac_pdu(sched_interface::dl_sched_data_t* data,
                                              dl_harq_proc*                     h,
                                              const rbgmask_t&                  user_mask,
                                              uint32_t                          tti_tx_dl,
                                              uint32_t                          ue_cc_idx,
                                              uint32_t                          cfi,
                                              uint32_t                          tb);

  std::pair<int, int> compute_mcs_and_tbs(uint32_t               ue_cc_idx,
                                          uint32_t               tti_tx_dl,
                                          uint32_t               nof_alloc_prbs,
                                          uint32_t               cfi,
                                          const srslte_dci_dl_t& dci);

  bool needs_cqi_unlocked(uint32_t tti, uint32_t cc_idx, bool will_send = false);

  int generate_format1(uint32_t                          pid,
                       sched_interface::dl_sched_data_t* data,
                       uint32_t                          tti,
                       uint32_t                          cc_idx,
                       uint32_t                          cfi,
                       const rbgmask_t&                  user_mask);
  int generate_format2a(uint32_t                          pid,
                        sched_interface::dl_sched_data_t* data,
                        uint32_t                          tti,
                        uint32_t                          cc_idx,
                        uint32_t                          cfi,
                        const rbgmask_t&                  user_mask);
  int generate_format2(uint32_t                          pid,
                       sched_interface::dl_sched_data_t* data,
                       uint32_t                          tti,
                       uint32_t                          cc_idx,
                       uint32_t                          cfi,
                       const rbgmask_t&                  user_mask);

  /* Args */
  sched_interface::ue_cfg_t               cfg  = {};
  srslte_cell_t                           cell = {};
  srslte::log_ref                         log_h;
  const std::vector<sched_cell_params_t>* cell_params_list = nullptr;
  const sched_cell_params_t*              main_cc_params   = nullptr;

  /* Buffer states */
  bool           sr = false;
  lch_ue_manager lch_handler;

  int      power_headroom  = 0;
  uint32_t cqi_request_tti = 0;
  uint16_t rnti            = 0;
  uint32_t max_msg3retx    = 0;

  /* User State */
  int next_tpc_pusch = 0;
  int next_tpc_pucch = 0;

  bool phy_config_dedicated_enabled = false;

  srslte::tti_point        current_tti;
  std::vector<cc_sched_ue> carriers; ///< map of UE CellIndex to carrier configuration
};

using sched_ue_list = std::map<uint16_t, sched_ue>;

} // namespace srsenb

#endif // SRSENB_SCHEDULER_UE_H