idf.py menuconfig → Partition Table:
* Easiest: select Factory app, two OTA definitions (uses the built-in partitions_two_ota.csv).
* If you want your own CSV, select Custom partition table, then set Custom partition table filename to your file (e.g. partitions.csv). The Partition table filename entry is only used for the built-in presets; ignore it once you pick Custom.

```
idf.py -p /dev/ttyACM0 erase_flash && idf.py -p /dev/ttyACM0 flash 
```