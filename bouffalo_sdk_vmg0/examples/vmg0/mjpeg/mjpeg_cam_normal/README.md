# cam_normal
注意，使用该样例需要将bsp/common/image_sensor/gc0308.h文件中打开第129行，注释掉130行，
将摄像头配置为Y Cb Y Cr模式，

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|BL702/BL704/BL706 |        |
|BL616/BL618       |        |
|BL808             |        |

## Compile

- BL602/BL604

```
make CHIP=bl602 BOARD=bl602dk
```

- BL702/BL704/BL706

```
make CHIP=bl702 BOARD=bl702dk
```

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