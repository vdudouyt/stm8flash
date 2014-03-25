stm8flash
=========

This is a free and opensource software distributed under the terms of GNU General Public License v2.

It also seems to be the only program that's able to communicate through the SWIM interface of ST-LINKs under Linux as for March, 2014.


Synopsis
--------

```
stm8flash -c <stlink|stlinkv2> -p <partname> [-s flash|eeprom|0x8000] [-r|-w|-v] <filename>
```

Flash samples:
```nohighlight
./stm8flash -c stlink -p stm8s003 -w blinky.bin
./stm8flash -c stlink -p stm8s003 -w blinky.ihx
./stm8flash -c stlinkv2 -p stm8s003 -w blinky.ihx
./stm8flash -c stlink -p stm8s105 -w blinky.bin
./stm8flash -c stlinkv2 -p stm8l150 -w blinky.bin
```

EEPROM samples:
```nohighlight
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -r ee.bin
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -w ee.bin
./stm8flash -c stlinkv2 -p stm8s003 -s eeprom -v ee.bin
```

Support table
-------------

  * ST-Link V1: flash/eeprom/opt
  * ST-Link V2: flash2/eeprom2/opt2

| MCU      | flash | eeprom | opt  | flash2 | eeprom2 | opt2  |
|----------|-------|--------|------|--------|---------|-------|
| stm8s003 |  ok   |  FAIL  |  no  |  ok    |  ok     |  no   |
| stm8s103 |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8s105 |  ok   |  FAIL  |  no  |  ok    |  ok     |  no   |
| stm8s208 |  ?    |  ?     |  ?   |  ?     |  ?      |  ?    |
| stm8l150 |  ok   |  FAIL  |  no  |  ok    |  ok     |  no   |

Legend:

  * `ok` - full supported
  * `no` - not supported now
  * `?` - not tested
  * `fail` - not work. Need fix

