stm8flash
=========

This is a free and opensource software distributed under the terms of GNU General Public License v2.

It also seems to be the only program that's able to communicate through the SWIM interface of ST-LINKs under Linux as for March, 2014.

Synopsis:

```nohighlight
./stm8flash -c stlink -p stm8s003 -w blinky.bin
./stm8flash -c stlink -p stm8s003 -w blinky.ihx
./stm8flash -c stlinkv2 -p stm8s003 -w blinky.ihx
./stm8flash -c stlink -p stm8s105 -w blinky.bin
./stm8flash -c stlinkv2 -p stm8l150 -w blinky.bin
```

EEPROM:
```nohighlight
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -r ee.bin
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -w ee.bin
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -v ee.bin
```


Tested:

|          | ST-Link        | ST-Link V2     |
| MCU      | flash | eeprom | flash | eeprom |
----------------------------------------------
| stm8s003 |  ok   |  fail  |  ok   |  ok    |
| stm8l150 |  ok   |  fail  |  ok   |  ok    |
| stm8s105 |  ok   |  fail  |  ok   |  ok    |

