/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SRSLTE_ENB_STACK_BASE_H
#define SRSLTE_ENB_STACK_BASE_H

#include "srslte/interfaces/enb_interfaces.h"
#include "srsue/hdr/stack/upper/gw.h"
#include <string>

namespace srsenb {

typedef struct {
  bool        enable;
  std::string filename;
} pcap_args_t;

typedef struct {
  bool        enable;
  std::string m1u_multiaddr;
  std::string m1u_if_addr;
  uint16_t    mcs;
} embms_args_t;

typedef struct {
  std::string mac_level;
  std::string rlc_level;
  std::string pdcp_level;
  std::string rrc_level;
  std::string gtpu_level;
  std::string s1ap_level;
  std::string stack_level;

  int mac_hex_limit;
  int rlc_hex_limit;
  int pdcp_hex_limit;
  int rrc_hex_limit;
  int gtpu_hex_limit;
  int s1ap_hex_limit;
  int stack_hex_limit;
} stack_log_args_t;

// Expert arguments to create GW without core NW
typedef struct {
  std::string      ip_addr;
  srsue::gw_args_t gw_args;
  uint8_t          drb_lcid;
  uint16_t         rnti;
} core_less_args_t;

typedef struct {
  std::string      type;
  uint32_t         sync_queue_size; // Max allowed difference between PHY and Stack clocks (in TTI)
  mac_args_t       mac;
  s1ap_args_t      s1ap;
  pcap_args_t      mac_pcap;
  pcap_args_t      s1ap_pcap;
  stack_log_args_t log;
  embms_args_t     embms;
  core_less_args_t coreless;
} stack_args_t;

struct stack_metrics_t;

class enb_stack_base
{
public:
  virtual ~enb_stack_base() = default;

  virtual std::string get_type() = 0;

  virtual void stop() = 0;

  // eNB metrics interface
  virtual bool get_metrics(stack_metrics_t* metrics) = 0;
  virtual void cmd_cell_earfcn(uint32_t cell_id, uint32_t dl_earfcn, uint32_t ul_earfcn) = 0;
  virtual void cmd_handover(uint32_t cell1_id, uint32_t cell2_id) = 0;
};

} // namespace srsenb

#endif // SRSLTE_ENB_STACK_BASE_H
