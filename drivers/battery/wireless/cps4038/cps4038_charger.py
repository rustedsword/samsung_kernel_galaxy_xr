config = {
    "build": {
        "file": "cps4038_charger.py",
        "location": [
            {
                "dst": "drivers/battery/wireless/",
                "src": "cps4038* Kconfig:update Makefile:update",
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "*.dtsi",
            },
        ],
        "path": "battery/suwon/wireless/cps4038",
    },
    "features": [
        {
            "configs": {
                "False": [
                    	"CONFIG_WIRELESS_CHARGER_CPS4038=y",
		],
                "True": [
                    	"CONFIG_WIRELESS_CHARGER_CPS4038=m",
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
            "label": "Use GPIO PING_NEN",
            "symbol": "${feature_use_gpio_ping_nen}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "Use GPIO PDET_B",
            "symbol": "${feature_use_gpio_pdet_b}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "Use GPIO MAG_DET",
            "symbol": "${feature_use_gpio_mag_det}",
            "type": "boolean",
            "value": False,
        },
        {
            "label": "Use GPIO WPC_MODE",
            "symbol": "${feature_use_gpio_wpc_mode}",
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
                    "CONFIG_WIRELESS_RX_PHM_CTRL=y"
                ],
            },
            "label": "WIRELESS_RX_PHM_CTRL",
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
        "name": "MFC Wireless Charger(CPS4038)",
        "product": "CPS",
        "type": "CHARGER",
        "uuid": "93a1ae45-bf5c-4f30-b551-a7fbde9560f3",
        "variant": "CPS4038",
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
                        "symbol": "${cps_det_gpio}",
                    },
                    {
                        "label": "GPIO: WPC_INT",
                        "parent": "${mfc_irq_parent}",
                        "properties": [],
                        "symbol": "${cps_irq_gpio}",
                    },
                    {
                        "label": "GPIO: WPC_EN",
                        "parent": "${wpc_en_parent}",
                        "properties": [],
                        "symbol": "${cps_en_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO PING_NEN']",
                        "label": "GPIO: PING_NEN",
                        "parent": "${ping_nen_parent}",
                        "properties": [],
                        "symbol": "${cps_ping_nen_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO PDET_B']", 
                        "label": "GPIO: PDET_B",
                        "parent": "${pdet_b_parent}",
                        "properties": [],
                        "symbol": "${cps_pdet_b_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO WPC_PDRC']",
                        "label": "GPIO: WPC_PDRC",
                        "parent": "${wpc_pdrc_parent}",
                        "properties": [],
                        "symbol": "${cps_pdrc_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO MST_PWR_EN']",
                        "label": "GPIO: MST_PWR_EN",
                        "parent": "${mst_pwr_en_parent}",
                        "properties": [],
                        "symbol": "${cps_pwr_en_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO COIL_SW_EN']",
                        "label": "GPIO: COIL_SW_EN",
                        "parent": "${coil_sw_en_parent}",
                        "properties": [],
                        "symbol": "${coil_sw_en_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO MAG_DET']",
                        "label": "GPIO: MAG_DET",
                        "parent": "${mag_det_parent}",
                        "properties": [],
                        "symbol": "${cps_mag_det_gpio}",
                    },
                    {
                        "depend": "feature['Use GPIO WPC_MODE']",
                        "label": "GPIO: WPC_MODE",
                        "parent": "${wpc_mode_parent}",
                        "properties": [],
                        "symbol": "${cps_wpc_mode_gpio}",
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
    "kunit_test": {
        "build": {
            "location": [
                {
                    "dst": "drivers/battery/wireless/",
                    "src": "cps4038* Kconfig:update Makefile:update",
                },
                {
                    "dst": "drivers/battery/wireless/kunit_test/",
                    "src": "kunit_test/*.c kunit_test/*.h kunit_test/Makefile:cp",
                },
                {
                    "dst": "drivers/battery/common/",
                    "src": "../../common/*.c ../../common/*.h ../../common/Kconfig:update ../../common/Makefile:update",
                },
                {
                    "dst": "drivers/battery/common/kunit_test/",
                    "src": "../../common/kunit_test/*.c ../../common/kunit_test/*.h ../../common/kunit_test/Makefile",
                },
                {
                    "dst": "include/linux/battery/",
                    "src": "../../core/*.h ../../common/linux/*.h",
                },
                {
                    "dst": "include/dt-bindings/battery/",
                    "src": "../../common/dt-bindings/*.h",
                },
            ]
        },
        "features": [
            {
                "label": "default",
                "configs": {
                    "False": [],
                    "True": [],
                    },
                "type": "boolean",
            },
            {
                "label": "uml_test",
                "configs": {
                    "False": [],
                    "True": [
                        "CONFIG_I2C=y",
                        "CONFIG_IIO=y",
                        "CONFIG_POWER_SUPPLY=y",
                        "CONFIG_BATTERY_SAMSUNG=y",
                        "CONFIG_WIRELESS_CHARGING=y",
                        "CONFIG_WIRELESS_CHARGER_CPS4038=y",
                        "CONFIG_CPS4038_CHARGER_TEST_FOR_ONLY_UML=y",
                        ],
                    },
                "type": "boolean",
            },
        ]

    },
}


def load():

    return config


if __name__ == "__main__":
    print(load())
