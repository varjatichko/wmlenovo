/*
 *    wmbatteries - A dockapp to monitor ACPI status of two batteries
 *    Copyright (C) 2003  Florian Krohs <krohs@uni.de>

 *    Based on work by Thomas Nemeth <tnemeth@free.fr>
 *    Copyright (C) 2002  Thomas Nemeth <tnemeth@free.fr>
 *    and on work by Seiichi SATO <ssato@sh.rim.or.jp>
 *    Copyright (C) 2001,2002  Seiichi SATO <ssato@sh.rim.or.jp>
 *    and on work by Mark Staggs <me@markstaggs.net>
 *    Copyright (C) 2002  Mark Staggs <me@markstaggs.net>

 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.

 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.

 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define THERMAL_FILE "/proc/acpi/thermal_zone/THM0/temperature"
#define BAT0_STATE_FILE "/proc/acpi/battery/BAT0/state"
#define BAT1_STATE_FILE "/proc/acpi/battery/BAT1/state"
#define BAT0_INFO_FILE "/proc/acpi/battery/BAT0/info"
#define BAT1_INFO_FILE "/proc/acpi/battery/BAT1/info"
#define AC_STATE_FILE "/proc/acpi/ac_adapter/AC/state"
#define IBM_FAN_FILE "/proc/acpi/ibm/fan"
#define IBM_TEMP_FILE "/proc/acpi/ibm/thermal"
#define CPU_PERF_FILE "/proc/acpi/processor/CPU0/performance"
#define I8K_FILE "/proc/i8k"
/*
    $ cat /proc/i8k
    1.0 A17 2J59L02 52 2 1 8040 6420 1 2

The fields read from /proc/i8k are:

    1.0 A17 2J59L02 52 2 1 8040 6420 1 2
    |   |   |       |  | | |    |    | |
    |   |   |       |  | | |    |    | +------- 10. buttons status
    |   |   |       |  | | |    |    +--------- 9.  ac status
    |   |   |       |  | | |    +-------------- 8.  right fan rpm
    |   |   |       |  | | +------------------- 7.  left fan rpm
    |   |   |       |  | +--------------------- 6.  right fan status
    |   |   |       |  +----------------------- 5.  left fan status
    |   |   |       +-------------------------- 4.  CPU temperature (Celsius)
    |   |   +---------------------------------- 3.  Dell service tag (later known as 'serial number')
    |   +-------------------------------------- 2.  BIOS version
    +------------------------------------------ 1.  /proc/i8k format version
*/


#define SYS_THERMAL_FILE "/sys/class/hwmon/hwmon0/temp1_input" // XXY00
#define SYS_THERMAL_V_FILE "/sys/class/hwmon/hwmon0/temp4_input" // XXY00
#define SYS_THERMAL_M_FILE "/sys/class/hwmon/hwmon0/temp5_input" // XXY00
#define SYS_BAT0_STATE_FILE "/sys/class/power_supply/BAT0/status" // Unknown/Discharging/Charging/Full
#define SYS_BAT1_STATE_FILE "/sys/class/power_supply/BAT1/status"
#define SYS_BAT0_CHARGE_NOW_FILE "/sys/class/power_supply/BAT0/charge_now"
#define SYS_BAT1_CHARGE_NOW_FILE "/sys/class/power_supply/BAT1/charge_now"
#define SYS_BAT0_CURRENT_NOW_FILE "/sys/class/power_supply/BAT0/current_now"
#define SYS_BAT1_CURRENT_NOW_FILE "/sys/class/power_supply/BAT1/current_now"
#define SYS_BAT0_CHARGE_FULL_FILE "/sys/class/power_supply/BAT0/charge_full"
#define SYS_BAT1_CHARGE_FULL_FILE "/sys/class/power_supply/BAT1/charge_full"
#define SYS_AC_STATE_FILE "/sys/class/power_supply/AC/online" // 1/0
#define SYS_IBM_FAN_FILE "/sys/devices/platform/thinkpad_hwmon/fan1_input" //XXXX Value in RPM
#define SYS_IBM_CPU_TEMP_FILE "/sys/devices/platform/thinkpad_hwmon/temp1_input"
#define SYS_IBM_MB_TEMP_FILE "/sys/devices/platform/thinkpad_hwmon/temp3_input"
#define SYS_IBM_GPU_TEMP_FILE "/sys/devices/platform/thinkpad_hwmon/temp2_input"
#define SYS_CPU_PERF_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq" //xXXXXXX cpuinfo_cur_freq require root priviledgies so using scaling one instead. Value in KHz.
