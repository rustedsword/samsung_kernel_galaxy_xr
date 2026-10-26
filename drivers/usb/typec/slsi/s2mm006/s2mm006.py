# Please uncomment and modify if you want to use
# Remove comment when you done.

config = {
    "header": {
        "uuid": "1a25d345-edc2-4729-a587-8b0df35aea66",
        "type": "USB",
        "vendor": "SLSI",
        "product": "s2mm006",
        "variant": "s2mm006 #T",
        "name": "s2mm006",
        "required": [
            [
                "84ed2d51-2511-4575-8c31-eaa1292bb4f0",
                "7cdb049c-70d3-4ad1-93d1-88ad73815141",
                "c72f0780-8e0b-4296-aecb-d9a3324d28d5",
            ]
        ],
    },
    "build": {
        "path": "usb/TIGER/typec/slsi/s2mm006",
        "file": "s2mm006.py",
        "location": [
            {
                "dst": "drivers/usb/typec/slsi/s2mm006/",
                "src": "*.c Kconfig Makefile",
            },
            {
                "dst": "drivers/usb/typec/slsi/s2mm006/",
                "src": "*.h",
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "s2mm006.dtsi",
            },
        ],
    },
    "features": [
        {
            "label": "BUILD TYPE",
            "configs": {
                "not set": [
                    "#CONFIG_PDIC_S2MM006 is not set",
                ],
                "module": [
                    "CONFIG_PDIC_S2MM006=m",
                ],
                "built-in": [
                    "CONFIG_PDIC_S2MM006=y",
                ],
            },
            "list_value": [
              "not set",
              "module",
              "built-in",
            ],
            "type": "list",
            "value": "built-in",
        },
        {
            "label": "FEATURES",
            "configs": {
                "MUIC_S2MM006": [
                   "CONFIG_MUIC_S2MM006=y"
                ],
                "PDIC_ALTERNATE_MODE": [
                   "CONFIG_PDIC_ALTERNATE_MODE=y"
                ],
                "USE FW_DOWNLOAD": [
                    "CONFIG_S2MM006_FW_DOWNLOAD=y"
                ],
            },
            "list_value": [
              "MUIC_S2MM006",
              "PDIC_ALTERNATE_MODE",
              "USE FW_DOWNLOAD",
            ],
            "type": "multicheck",
            "value": [],
        },
        {
            "label": "WATER DETECT SUPPORT",
            "symbol": "${water_detect_support}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "PDIC I2C clock",
            "help": "This applies only to lsi",
            "symbol": "${pdic_i2c_clk}",
            "type": "string",
            "value": "400000"
        },
    ],
    "interfaces": {
        "inports": [
            {
                "labels": [
                    {
                        "label": "GPIO: PD_INT",
                        "parent": "${pd_irq_parent}",
                        "properties": [],
                        "symbol": "${pd_irq_gpio}",
                    },
                    {
                        "label": "GPIO: HRST_OM",
                        "parent": "${hrst_om_parent}",
                        "properties": [],
                        "symbol": "${hrst_om_gpio}",
                    },
                    {
                        "label": "GPIO: VBUS_DISCHARGING",
                        "parent": "${vbus_discharging_parent}",
                        "properties": [],
                        "symbol": "${vbus_discharging_gpio}",
                    },
                    {
                        "label": "GPIO: USB3.1_CON_SEL",
                        "parent": "${cc_dir_parent}",
                        "properties": [],
                        "symbol": "${cc_dir_gpio}",
                    },
                ],
                "type": "gpio",
            },
            {
                "labels": [
                    {
                        "label": "I2C: S2MM006",
                        "properties": [],
                        "symbol": "${pdic_i2c}",
                    }
                ],
                "type": "i2c",
            },
        ],
    },
}


def load():
    return config

if __name__ == '__main__':
    print(load())
