# Please uncomment and modify if you want to use

config = {
    "header": {
        "uuid": "05c73b89-e16d-4c22-83b4-65f32f7d6051",
        "type": "OPTICS",
        "vendor": "Sitronix",
        "product": "stk68210",
        "variant": "stk68210",
        "name": "stk68210",
    },
    "build": {
        "path": "camera/common/optics/stk6d2x",  # path for reach this file
        "file": "stk6d2x.py",  # file name
        "location": [
            ##### COMMON #####
            {
                "src": "*.c *.h Makefile:update Kconfig:update",
                "dst": "drivers/optics/",
            },
            {
                "depend": "feature['USE OSC CALIBRATION'] == 'none'",
                "src": "stk6d2x.dtsi",
                "dst": "arch/arm64/boot/dts/samsung/stk6d2x.dtsi",
            },
            ##### PWM Calibration ######
            {
                "depend": "feature['USE OSC CALIBRATION'] == 'pwm'",
                "src": "stk6d2x_cal_pwm.c",
                "dst": "drivers/optics/stk6d2x_cal.c"
            },
            {
                "depend": "feature['USE OSC CALIBRATION'] == 'pwm'",
                "src": "stk6d2x_cal_pwm.dtsi",
                "dst": "arch/arm64/boot/dts/samsung/stk6d2x.dtsi",
            },
            ###### GPIO based PWM Calibration ######
            {
                "depend": "feature['USE OSC CALIBRATION'] == 'gpio'",
                "src": "stk6d2x_cal_gpio.c",
                "dst": "drivers/optics/stk6d2x_cal.c"
            },
            {
                "depend": "feature['USE OSC CALIBRATION'] == 'gpio'",
                "src": "stk6d2x_cal_gpio.dtsi",
                "dst": "arch/arm64/boot/dts/samsung/stk6d2x.dtsi",
            },
       ],
    },
    "features": [
        {
            "label": "General",
            "type": "boolean",
            # Add configs for each selection
            "configs": {
                "True": [],
                "False": [],
            },
            # Add default value, it will be set if user does not select.
            "value": True,
        },
        {
            "label": "Build Type",
            "configs": {
                "not set": ["# CONFIG_SENSORS_STK6D2X is not set"],
                "module": ["CONFIG_SENSORS_STK6D2X=m"],
                "built-in": ["CONFIG_SENSORS_STK6D2X=y"],
            },
            "list_value": [
                "not set",
                "module",
                "built-in",
            ],
            "type": "list",
            "value": "module",
        },
        {
            "label": "USE VDD REGULATOR",
            "symbol": "${use_vdd_regulator}",
            "type": "boolean",
            "value": True,
        },
        {
            "label": "CONTROL VBUS REGULATOR",
            "symbol": "${use_vbus_regulator}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "USE I3C INTERFACE",
            "symbol": "${use_i3c_interface}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "USE OSC CALIBRATION",
            "symbol": "${use_osc_calibration}",
            "list_value": [
                "none",
                "pwm",
                "gpio",
            ],
            "configs": {
                "none": ["# CONFIG_FLICKER_PWM_CALIBRATION is not set"],
                "pwm": ["CONFIG_FLICKER_PWM_CALIBRATION=y"],
                "gpio": ["CONFIG_FLICKER_PWM_CALIBRATION=y"],
            },
            "type": "list",
            "value": "none",
        },
        {
            "label": "PWM Period",
            "symbol": "${pwm_period}",
            "regex": r"^[0-9]+$",
            "type": "string",
            "value": "25000000",
        },
        {
            "label": "USE EXTERNAL CLK",
            "symbol": "${use_external_clk}",
            "type": "boolean",
            "value": True,
        },
    ],
    "interfaces": {
        "inports": [
            {
                "type": "gpio",
                "labels": [
                    {
                        "label": "GPIO: MCLK_FLICKER",
                        "properties": [],
                        "symbol": "${mclk_flicker}",
                        "parent": "${mclk_flicker_parent}",
                    },
                    {
                        "label": "GPIO: PWM_FLICKER",
                        "properties": [],
                        "symbol": "${pwm_flicker}",
                        "parent": "${pwm_flicker_parent}",
                        "depend": "feature['USE OSC CALIBRATION'] == 'gpio'",
                    },
                ],
            },
            {
                "type": "pwm",
                "labels": [
                    {
                        "label": "PWM: CAL_PWM",
                        "symbol": "${pwm_flicker}",
                        "depend": "feature['USE OSC CALIBRATION'] == 'pwm'",
                    }
                ],
            },
            {
                "type": "i2c",
                "labels": [
                    {
                        "label": "I2C: FLICKER",
                        "properties": [],
                        "symbol": "${flicker_i2c}",
                        "depend": "feature['USE I3C INTERFACE'] == False",
                    },
                ],
            },
            {
                "type": "i3c",
                "labels": [
                    {
                        "label": "I3C: FLICKER",
                        "properties": [],
                        "symbol": "${flicker_i2c}",
                        "depend": "feature['USE I3C INTERFACE'] == True",
                    },
                ],
            },
            {
                "type": "regulator",
                "labels": [
                    {
                        "label": "LDO: FLICKER_VDD",
                        "properties": [],
                        "symbol": "${flicker_ldo_vdd}",
                        "depend": "feature['USE VDD REGULATOR']",
                    },
                    {
                        "label": "LDO: FLICKER_VBUS",
                        "properties": [],
                        "symbol": "${flicker_ldo_vbus}",
                        "depend": "feature['CONTROL VBUS REGULATOR']",
                    },
                ],
            },
        ],
    },
}


def load():
    return config

if __name__ == '__main__':
    print(load())
