config = {
    "build": {
        "file": "s2miw04_charger.py",
        "location": [
            {
                "dst": "drivers/battery/wireless/",
                "src": "s2miw04* Kconfig:update Makefile:update",
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "*.dtsi",
            },
        ],
        "path": "battery/suwon/wireless/s2miw04",
    },
    "features": [
        {
            "configs": {
                "False": [
                    	"CONFIG_WIRELESS_CHARGER_S2MIW04=y",
		],
                "True": [
                    	"CONFIG_WIRELESS_CHARGER_S2MIW04=m",
                ],
            },
            "label": "GKI_MODULE_BUILD",
            "type": "boolean",
            "value": True,
        },
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_WIRELESS_AUTH=y",
                    "CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE=y",
                    "CONFIG_WIRELESS_TX_MODE=y",
                    "CONFIG_WIRELESS_FIRMWARE_UPDATE=y",
                    "CONFIG_WIRELESS_IC_PARAM=y",
                ],
            },
            "label": "General",
            "type": "boolean",
            "value": True,
        },
        {
            "label": "Use GPIO WPC_PDRC",
            "symbol": "${feature_use_gpio_wpc_pdrc}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "Use GPIO MST_PWR_EN",
            "symbol": "${feature_use_gpio_mst_pwr_en}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "Use GPIO COIL_SW_EN",
            "symbol": "${feature_use_gpio_coil_sw_en}",
            "type": "boolean",
            "value": False,
        },
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_TX_GEAR_PHM_VOUT_CTRL=y"
                ],
            },
            "label": "TX_GEAR_PHM_VOUT_CTRL",
            "type": "boolean",
            "value": False,
        },
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_TX_GEAR_AOV=y"
                ],
            },
            "label": "TX_GEAR_AOV",
            "type": "boolean",
            "value": False,
        },
        {
            "configs": {
                "False": [],
                "True": [
                    "CONFIG_ENABLE_WIRELESS_IRQ_IN_SLEEP=y"
                ],
            },
            "label": "ENABLE_WIRELESS_IRQ_IN_SLEEP",
            "type": "boolean",
            "value": False,
        }
    ],
    "header": {
        "name": "MFC Wireless Charger(S2MIW04)",
        "product": "S.LSI",
        "type": "CHARGER",
        "uuid": "889aaf8c-e23e-463f-9648-cfb42b7d8743",
        "variant": "S2MIW04",
        "vendor": "Suwon",
    },
    "interfaces": {
        "inports": [
            {
                "labels": [
                    {
                        "label": "GPIO: WPC_DET",
                        "parent": "${wpc_det_parent}",
                        "properties": [],
                        "symbol": "${lsi_det_gpio}",
                    },
                    {
                        "label": "GPIO: WPC_INT",
                        "parent": "${mfc_irq_parent}",
                        "properties": [],
                        "symbol": "${lsi_irq_gpio}",
                    },
                    {
                        "label": "GPIO: WPC_EN",
                        "parent": "${wpc_en_parent}",
                        "properties": [],
                        "symbol": "${lsi_en_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO WPC_PDRC']",
                        "label": "GPIO: WPC_PDRC",
                        "parent": "${wpc_pdrc_parent}",
                        "properties": [],
                        "symbol": "${lsi_pdrc_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO MST_PWR_EN']",
                        "label": "GPIO: MST_PWR_EN",
                        "parent": "${mst_pwr_en_parent}",
                        "properties": [],
                        "symbol": "${lsi_pwr_en_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO COIL_SW_EN']",
                        "label": "GPIO: COIL_SW_EN",
                        "parent": "${coil_sw_en_parent}",
                        "properties": [],
                        "symbol": "${coil_sw_en_gpio}",
                    },
                ],
                "type": "gpio",
            },
            {
                "labels": [
                    {
                        "label": "I2C: WIRELESS",
                        "properties": [],
                        "symbol": "${wireless_i2c}",
                    }
                ],
                "type": "i2c",
            },
            {
                "labels": [
                    {
                        "label": "charger_name",
                        "properties": [],
                        "symbol": "${charger_name}",
                    }
                ],
                "type": "charger_name",
            },
            {
                "labels": [
                    {
                        "label": "fuelgauge_name",
                        "properties": [],
                        "symbol": "${fuelgauge_name}",
                    }
                ],
                "type": "fuelgauge_name",
            },
        ]
    },
}


def load():

    return config


if __name__ == "__main__":
    print(load())
