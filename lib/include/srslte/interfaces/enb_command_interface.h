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

#ifndef SRSLTE_ENB_COMMAND_INTERFACE_H
#define SRSLTE_ENB_COMMAND_INTERFACE_H

#include <cstdint>

namespace srsenb {
class enb_command_interface
{
public:
  /**
   * Sets the relative gain of a cell from it's index (following rr.conf) order.
   * @param cell_id Provides a cell identifier
   * @param gain Relative gain
   */
  virtual void cmd_cell_gain(uint32_t cell_id, float gain) = 0;

  /**
   * Sets the earfcn a cell from it's index (following rr.conf) order.
   * @param cell_id Provides a cell identifier
   * @param dl_earfcn Earfcn in downlink
   * @param ul_earfcn Earfcn in uplink
   */
  virtual void cmd_cell_earfcn(uint32_t cell_id, uint32_t dl_earfcn, uint32_t ul_earfcn) = 0;

  /**
   * Force users in cell1 to handover to cell2.
   * @param cell1_id id of the source cell
   * @param cell2_id id of the target (destination) cell
   */
  virtual void cmd_handover(uint32_t cell1_id, uint32_t cell2_id) = 0;
};
} // namespace srsenb

#endif // SRSLTE_ENB_COMMAND_INTERFACE_H
