stm8flash
=========

This is free and opensource software distributed under the terms of the GNU General Public License,
either version 2 of the License, or (at your option) any later version.

For years, it was the only program that's able to communicate through the SWIM interface of ST-LINKs under Linux.

Since 2018, OpenOCD also offers the basic functionality, and also has support for on-target debugging.
As of early 2018, stm8flash has wider device support, and better support for memory read/write operations.

This is a fork and rewrite of https://github.com/vdudouyt/stm8flash 

Synopsis
--------

```nohighlight
Usage: stm8flash [OPTION...] [SLICE] [FILE]
 This tool is used to read and write the various memories of the stm8 MCUs.

 FILE gives the file path where data is read/written. If FILE is not given stdin/stdout is used instead.

 SLICE Specifies the range of address that are to be read or written in [START:STOP] format. If START or STOP is preceded by `0x` it is taken as a hexidecimal number. The bytes at addresss START up to but not including STOP will be read/written. If START or STOP is ommited `start of file` and `end of file` is understood instead. If STOP is preceded by `+` it is interpreted relative to START and gives the number of bytes read/written.
 Alternatively you may specify `opt`, `eeprom`, `flash` or `ram`
  -r            Read SLICE from MCU to FILE
  -v            Read SLICE from MCU and compare to FILE
  -w            Write SLICE from FILE to MCU
  -u            Remove readout protection to unlock the MCU before other r/w operations.
  -k            Apply readout protection to lock the MCU after other r/w operations.

  -f            Skip the MCU size checks and force operation.
  -n            Do not perform MCU reset after completion.
  -p PART       Specify the part number or family.
  -l            List all supported parts and exit.
  -S SERIAL     Use a specific programmer by serial number.
  -L            List all programmering adapers and exit.
  -x FORMAT     Assume the FORMAT of FILE. Supported values are ihex, srec, bin.
  -g            increase debug level.

  -?            Give this help list
  -V            Print program version

Mandatory or optional arguments to long options are also mandatory or optionalfor any corresponding short options.

The help text following options

This is used after all other documentation; text is zero for this key

Report bugs here https://github.com/schneidersoft/stm8flash.
```

The supported file types are Intel Hex, Motorola S-Record and Raw Binary. The type is detected by the file extension of can be given explicitly with the -x option

Flash examples read and write verify:
```nohighlight
./stm8flash -p stm8s003f3 -r flash blinky.bin
./stm8flash -p stm8s003f3 -w flash blinky.ihx
./stm8flash -p stm8s003f3 -v flash blinky.ihx
```

EEPROM examples read and write verify:
```nohighlight
./stm8flash -p stm8s003f3 -r eeprom ee.bin
./stm8flash -p stm8s003f3 -w eeprom ee.bin
./stm8flash -p stm8s003f3 -v eeprom ee.bin 
```

Readout protection
-------------

There are two kinds of option bytes. Those where 0xAA disables ROP and those where anything other than 0xAA disables it.

The stm8l050j3 is an example where writing 0xAA to the option bytes disables ROP

The stm8s208mb is an example where writing 0x00 to the option bytes disables ROP

stm8flash will handle both cases.

However. On devices like the stm8s208mb, removing the ROP means clearing the option bytes and clearing the option bytes means that the flash is still locked.
So prior to being able to write the flash again, one needs to write valid option bytes. The default option bytes can be read from the datasheet.

The following should generally work to unlock the memories.
Problem is that

LOCK example:
```nohighlight
./stm8flash -p stm8s003f3 -k
```

UNLOCK example:
```nohighlight
./stm8flash -p stm8s003f3 -u

python3 -c "open('opt.bin', 'wb').write(b'\x00\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF')"
./stm8flash -p stm8s208mb -u -w opt opt.bin
```

Supported programming adapters
-------------
Currently only the stlinkv2 is supported.

Supported parts
-------------

stlux385 stlux???a stm8af526? stm8af528? stm8af52a? stm8af6213 stm8af6223 stm8af6223a stm8af6226 stm8af624? stm8af6266 stm8af6268 stm8af6269 stm8af628? stm8af62a? stm8al313? stm8al314?? stm8al316? stm8al318? stm8al31e8? stm8al3l4? stm8al3l6? stm8al3l8? stm8al3le8? stm8l001j3 stm8l050j3 stm8l051f3 stm8l052c6 stm8l052r8 stm8l101?2 stm8l101?3 stm8l101f1 stm8l151?2 stm8l151?3 stm8l151?4 stm8l151?6 stm8l151?8 stm8l152?4 stm8l152?6 stm8l152?8 stm8l162?8 stm8s001j3 stm8s003?3 stm8s005?6 stm8s007c8 stm8s103?3 stm8s103f2 stm8s105?4 stm8s105?6 stm8s207?6 stm8s207c8 stm8s207cb stm8s207k8 stm8s207m8 stm8s207mb stm8s207r8 stm8s207rb stm8s207s8 stm8s207sb stm8s208?8 stm8s208?b stm8s208c6 stm8s208r6 stm8s208s6 stm8s903?3 stm8splnb1 stm8tl5??4 stnrg???a
