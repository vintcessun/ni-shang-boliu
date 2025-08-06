# i2c_10_bit


## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|BL616/BL618       |        |
|BL808             |        |

## Compile

- BL616/BL618

```
make CHIP=bl616 BOARD=bl616dk
```

- BL808

```
make CHIP=bl808 BOARD=bl808dk CPU_ID=m0
make CHIP=bl808 BOARD=bl808dk CPU_ID=d0
```

## Flash

```
make flash CHIP=chip_name COMX=xxx # xxx is your com name
```

当前这个通过手动I2c实现了查询1转40的功能