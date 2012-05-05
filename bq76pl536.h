

/* Special addresses */

#define DISCOVERY_ADDRESS 0x00
#define BROADCAST         0x3F


#define RESET_COMMAND     0xA5

/*
    REGISTERS

        NAME            ADDR    ACCESS  RESET DESCRIPTION
                                ACCESS
*/
#define DEVICE_STATUS   0x00 /* R       0 Status register                      */
#define DS_ADDR_RQST    0x80 /*           1 = address assigned                 */
#define DS_FAULT        0x40 /*           Look in FAULT_STATUS for details     */
#define DS_ALERT        0x20 /*           Look in ALERT_STATUS for datails     */
#define DS_ECC_COR      0x08 /*           One bit err found and fixed in EPROM */
#define UVLO            0x04 /*           Under voltage                        */
#define CBT             0x02 /*           Cell balance timer is running        */
#define DRDY            0x01 /*           Data ready                           */
#define GPAI            0x01 /* R       0 GPAI measurement data                */
#define VCELL1          0x03 /* R       0 Cell 1 voltage data                  */
#define VCELL2          0x05 /* R       0 Cell 2 voltage data                  */
#define VCELL3          0x07 /* R       0 Cell 3 voltage data                  */
#define VCELL4          0x09 /* R       0 Cell 4 voltage data                  */
#define VCELL5          0x0B /* R       0 Cell 5 voltage data                  */
#define VCELL6          0x0D /* R       0 Cell 6 voltage data                  */
#define TEMPERATURE1    0x0F /* R       0 TS1+ to TS1– differential voltage    */
#define TEMPERATURE2    0x11 /* R       0 TS2+ to TS2– differential voltage    */
#define ALERT_STATUS    0x20 /* R/W  0x80 Indicates source of ALERT signal     */
#define AS_AR           0x80 /*           Address not set                      */
#define AS_PARITY       0x40 /*           Group3 protected regs are invalid    */
#define AS_ECC_ERR      0x20 /*           OTP-EPROM registers are not valid    */
#define AS_FORCE        0x10 /*           Set this bit to force a fault        */
#define AS_TSD          0x08 /*           Thermal shutdown                     */
#define AS_SLEEP        0x04 /*           Sleep was activated                  */
#define AS_OT2          0x02 /*           Over temperature - sensor 2          */
#define AS_OT1          0x01 /*           Over temperature - sensor 1          */
#define FAULT_STATUS    0x21 /* R/W  0x08 Indicates source of FAULT signal     */
#define FS_I_FAULT      0x20 /*           Internal reg consistancy check fail  */
#define FS_FORCE        0x10 /*           Set this bit to force a fault        */
#define FS_POR          0x08 /*           Power on reset                       */
#define FS_CRC          0x04 /*           CRC error                            */
#define FS_CUV          0x02 /*           Undervoltage                         */
#define FS_COV          0x01 /*           Overvoltage                          */
#define COV_FAULT       0x22 /* R       0 OV fault state                       */
#define CUV_FAULT       0x23 /* R       0 UV fault state                       */
#define PRESULT_A       0x24 /* R       0 Parity result protected reg (A)      */
#define PRESULT_B       0x25 /* R       0 Parity result protected reg (B)      */
#define ADC_CONTROL     0x30 /* R/W     0 ADC measurement control              */
#define AC_ADC_ON       0x40 /*           Leave the ADC subsystem on always    */
#define AC_TS2          0x20 /*           Enable temperature sensor 2          */
#define AC_TS1          0x10 /*           Enable temperature sensor 1          */
#define AC_GPAI         0x08 /*           Enable GPAI                          */
#define AC_CELL_SEL_1   0x00 /*           Select cell 1 only                   */
#define AC_CELL_SEL_2   0x01 /*           Select cell 1-2                      */
#define AC_CELL_SEL_3   0x02 /*           Select cell 1-3                      */
#define AC_CELL_SEL_4   0x03 /*           Select cell 1-4                      */
#define AC_CELL_SEL_5   0x04 /*           Select cell 1-5                      */
#define AC_CELL_SEL_6   0x05 /*           Select cell 1-6                      */
#define IO_CONTROL      0x31 /* R/W     0 I/O pin control                      */
#define IO_AUX          0x80 /*           Set to connect AUX pin to REG50      */
#define IO_GPIO_OUT     0x40 /*           1 sets GPIO pin high, 0 floats       */
#define IO_GPIO_IN      0x20 /*           Read GPIO pin                        */
#define IO_SLEEP        0x04 /*           Set sleep mode                       */
#define TS2             0x02 /*           1 = enable thermistor 1              */
#define TS1             0x01 /*           1 = enable thermistor 2              */
#define CB_CTRL         0x32 /* R/W     0 Controls cell-balancing outputs CBx  */
#define CB_TIME         0x33 /* R/W     0 CB control FETs maximum on time      */
#define ADC_CONVERT     0x34 /* R/W     0 ADC conversion start                 */
#define AC_CONV         0x01 /*           Start the conversion                 */
#define SHDW_CTRL       0x3a /* R/W     0 WRITE access to Group3 registers     */
#define SC_ENABLE       0x35 /*           Val to allow writing to 40-4F        */
#define ADDRESS_CONTROL 0x3b /* R/W     0 Address register                     */
#define AC_ADDR_RQST    0x80 /*           Address request bit                  */
#define RESET           0x3c /* W 0     0 RESET control register               */
#define TEST_SELECT     0x3d /* R/W     0 Test mode selection register         */
#define E_EN            0x3f /* R/W     0 EPROM programming mode enable        */
#define FUNCTION_CONFIG 0x40 /* R/W EPROM Default configuration of device      */
#define FC_ADCT3        0x00 /*           ADC time 3us                         */
#define FC_ADCT6        0x40 /*           ADC time 6us                         */
#define FC_ADCT12       0x80 /*           ADC time 12us                        */
#define FC_ADCT24       0xC0 /*           ADC time 24us                        */
#define FC_GPAI_REF     0x20 /*           GPAI ref     0=internal ADC 1=VREG50 */
#define FC_GPAI_SRC     0x10 /*           GPAI source  0=pins         1=brick  */
#define FC_CN1          0x08 /*           Cell count 1 0=6  1=5  2=4  3=3      */
#define FC_CN0          0x04 /*           Cell count 0                         */
#define IO_CONFIG       0x41 /* R/W EPROM I/O pin configuration                */
#define IC_CRC_DIS      0x01 /*           Disable CRC                          */
#define CONFIG_COV      0x42 /* R/W EPROM Overvoltage set point                */
#define COV_DISABLE     0x80 /*           Low 6 bits 0-63 * 50mV + 2V          */
#define COV_200         0x00
#define COV_205         0x01
#define COV_210         0x02
#define COV_215         0x03
#define COV_220         0x04
#define COV_225         0x05
#define COV_230         0x06
#define COV_235         0x07
#define COV_240         0x08
#define COV_245         0x09
#define COV_250         0x0A
#define COV_255         0x0B
#define COV_260         0x0C
#define COV_265         0x0D
#define COV_270         0x0E
#define COV_275         0x0F
#define COV_280         0x10
#define COV_285         0x11
#define COV_290         0x12
#define COV_295         0x13
#define COV_300         0x14
#define COV_305         0x15
#define COV_310         0x16
#define COV_315         0x17
#define COV_320         0x18
#define COV_325         0x19
#define COV_330         0x1A
#define COV_335         0x1B
#define COV_340         0x1C
#define COV_345         0x1D
#define COV_350         0x1E
#define COV_355         0x1F
#define COV_360         0x20
#define COV_365         0x21
#define COV_370         0x22
#define COV_375         0x23
#define COV_380         0x24
#define COV_385         0x25
#define COV_390         0x26
#define COV_395         0x27
#define COV_400         0x28
#define COV_405         0x29
#define COV_410         0x2A
#define COV_415         0x2B
#define COV_420         0x2C
#define COV_425         0x2D
#define COV_430         0x2E
#define COV_435         0x2F
#define COV_440         0x30
#define COV_445         0x31
#define COV_450         0x32
#define COV_455         0x33
#define COV_460         0x34
#define COV_465         0x35
#define COV_470         0x36
#define COV_475         0x37
#define COV_480         0x38
#define COV_485         0x39
#define COV_490         0x3A
#define COV_495         0x3B
#define COV_500         0x3C
#define COV_505         0x3D
#define COV_510         0x3E
#define COV_515         0x3F
#define COV_520         0x40
#define CONFIG_COVT     0x43 /* R/W EPROM Overvoltage time-delay filter        */
#define CC_USMS         0x80 /*           0 = us 1 = ms low 5 bits 0-31 * 100  */
#define CONFIG_UV       0x44 /* R/W EPROM Undervoltage setpoint                */
#define UV_DISABLE      0x80 /*           Low 5 bits 0-31 * 100mV + 0.7 V      */
#define CONFIG_CUTV     0x45 /* R/W EPROM Undervoltage time-delay filter       */
/* Use CC_USMS from above */
#define CONFIG_OT       0x46 /* R/W EPROM Overtemperature set point            */
#define OT_OFF          0x00 /*           Disabled                             */
#define OT_40C          0x01
#define OT_45C          0x02
#define OT_50C          0x03
#define OT_55C          0x04
#define OT_60C          0x05
#define OT_65C          0x06
#define OT_70C          0x07
#define OT_75C          0x08
#define OT_80C          0x09
#define OT_85C          0x0a
#define OT_90C          0x0b
#define CONFIG_OTT      0x47 /* R/W EPROM OT time-delay 0-255 * 10ms           */
#define USER1           0x48 /* R   EPROM User data register 1                 */
#define USER2           0x49 /* R   EPROM User data register 2                 */
#define USER3           0x4a /* R   EPROM User data register 3                 */
#define USER4           0x4b /* R   EPROM User data register 4                 */
