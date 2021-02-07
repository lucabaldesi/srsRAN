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

#include "srsenb/hdr/stack/upper/pdcp.h"
#include "srsenb/hdr/stack/upper/common_enb.h"

namespace srsenb {

pdcp::pdcp(srslte::task_sched_handle task_sched_, srslog::basic_logger& logger_) :
  task_sched(task_sched_), logger(logger_)
{}

void pdcp::init(rlc_interface_pdcp* rlc_, rrc_interface_pdcp* rrc_, gtpu_interface_pdcp* gtpu_)
{
  rlc  = rlc_;
  rrc  = rrc_;
  gtpu = gtpu_;
}

void pdcp::stop()
{
  for (std::map<uint32_t, user_interface>::iterator iter = users.begin(); iter != users.end(); ++iter) {
    clear_user(&iter->second);
  }
  users.clear();
}

void pdcp::add_user(uint16_t rnti)
{
  if (users.count(rnti) == 0) {
    srslte::pdcp* obj = new srslte::pdcp(task_sched, logger.id().c_str());
    obj->init(&users[rnti].rlc_itf, &users[rnti].rrc_itf, &users[rnti].gtpu_itf);
    users[rnti].rlc_itf.rnti  = rnti;
    users[rnti].gtpu_itf.rnti = rnti;
    users[rnti].rrc_itf.rnti  = rnti;

    users[rnti].rrc_itf.rrc   = rrc;
    users[rnti].rlc_itf.rlc   = rlc;
    users[rnti].gtpu_itf.gtpu = gtpu;
    users[rnti].pdcp          = obj;
  }
}

// Private unlocked deallocation of user
void pdcp::clear_user(user_interface* ue)
{
  ue->pdcp->stop();
  delete ue->pdcp;
  ue->pdcp = NULL;
}

void pdcp::rem_user(uint16_t rnti)
{
  if (users.count(rnti)) {
    clear_user(&users[rnti]);
    users.erase(rnti);
  }
}

void pdcp::add_bearer(uint16_t rnti, uint32_t lcid, srslte::pdcp_config_t cfg)
{
  if (users.count(rnti)) {
    if (rnti != SRSLTE_MRNTI) {
      users[rnti].pdcp->add_bearer(lcid, cfg);
    } else {
      users[rnti].pdcp->add_bearer_mrb(lcid, cfg);
    }
  }
}

void pdcp::del_bearer(uint16_t rnti, uint32_t lcid)
{
  if (users.count(rnti)) {
    users[rnti].pdcp->del_bearer(lcid);
  }
}

void pdcp::reset(uint16_t rnti)
{
  if (users.count(rnti)) {
    users[rnti].pdcp->reset();
  }
}

void pdcp::config_security(uint16_t rnti, uint32_t lcid, srslte::as_security_config_t sec_cfg)
{
  if (users.count(rnti)) {
    users[rnti].pdcp->config_security(lcid, sec_cfg);
  }
}

void pdcp::enable_integrity(uint16_t rnti, uint32_t lcid)
{
  users[rnti].pdcp->enable_integrity(lcid, srslte::DIRECTION_TXRX);
}

void pdcp::enable_encryption(uint16_t rnti, uint32_t lcid)
{
  users[rnti].pdcp->enable_encryption(lcid, srslte::DIRECTION_TXRX);
}

bool pdcp::get_bearer_state(uint16_t rnti, uint32_t lcid, srslte::pdcp_lte_state_t* state)
{
  if (users.count(rnti) == 0) {
    return false;
  }
  return users[rnti].pdcp->get_bearer_state(lcid, state);
}

bool pdcp::set_bearer_state(uint16_t rnti, uint32_t lcid, const srslte::pdcp_lte_state_t& state)
{
  if (users.count(rnti) == 0) {
    return false;
  }
  return users[rnti].pdcp->set_bearer_state(lcid, state);
}

void pdcp::reestablish(uint16_t rnti)
{
  if (users.count(rnti) == 0) {
    return;
  }
  users[rnti].pdcp->reestablish();
}

void pdcp::write_pdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  if (users.count(rnti)) {
    users[rnti].pdcp->write_pdu(lcid, std::move(sdu));
  }
}

void pdcp::notify_delivery(uint16_t rnti, uint32_t lcid, const std::vector<uint32_t>& pdcp_sns)
{
  if (users.count(rnti)) {
    users[rnti].pdcp->notify_delivery(lcid, pdcp_sns);
  }
}

void pdcp::write_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu, int pdcp_sn)
{
  if (users.count(rnti)) {
    if (rnti != SRSLTE_MRNTI) {
      // TODO: Handle PDCP SN coming from GTPU
      users[rnti].pdcp->write_sdu(lcid, std::move(sdu));
    } else {
      users[rnti].pdcp->write_sdu_mch(lcid, std::move(sdu));
    }
  }
}

std::map<uint32_t, srslte::unique_byte_buffer_t> pdcp::get_buffered_pdus(uint16_t rnti, uint32_t lcid)
{
  if (users.count(rnti)) {
    return users[rnti].pdcp->get_buffered_pdus(lcid);
  }
  return {};
}

void pdcp::user_interface_gtpu::write_pdu(uint32_t lcid, srslte::unique_byte_buffer_t pdu)
{
  gtpu->write_pdu(rnti, lcid, std::move(pdu));
}

void pdcp::user_interface_rlc::write_sdu(uint32_t lcid, srslte::unique_byte_buffer_t sdu)
{
  rlc->write_sdu(rnti, lcid, std::move(sdu));
}

void pdcp::user_interface_rlc::discard_sdu(uint32_t lcid, uint32_t discard_sn)
{
  rlc->discard_sdu(rnti, lcid, discard_sn);
}

bool pdcp::user_interface_rlc::rb_is_um(uint32_t lcid)
{
  return rlc->rb_is_um(rnti, lcid);
}

bool pdcp::user_interface_rlc::sdu_queue_is_full(uint32_t lcid)
{
  return rlc->sdu_queue_is_full(rnti, lcid);
}

void pdcp::user_interface_rrc::write_pdu(uint32_t lcid, srslte::unique_byte_buffer_t pdu)
{
  rrc->write_pdu(rnti, lcid, std::move(pdu));
}

void pdcp::user_interface_rrc::write_pdu_bcch_bch(srslte::unique_byte_buffer_t pdu)
{
  ERROR("Error: Received BCCH from ue=%d", rnti);
}

void pdcp::user_interface_rrc::write_pdu_bcch_dlsch(srslte::unique_byte_buffer_t pdu)
{
  ERROR("Error: Received BCCH from ue=%d", rnti);
}

void pdcp::user_interface_rrc::write_pdu_pcch(srslte::unique_byte_buffer_t pdu)
{
  ERROR("Error: Received PCCH from ue=%d", rnti);
}

std::string pdcp::user_interface_rrc::get_rb_name(uint32_t lcid)
{
  return to_string((rb_id_t)lcid);
}

} // namespace srsenb
