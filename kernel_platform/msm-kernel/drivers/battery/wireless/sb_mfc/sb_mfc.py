config = {
	"header": {
		"uuid": "7a0e798d-846a-4ad8-84d3-bd4fe738347b",
		"name": "SB-MFC",
		"type": "CHARGER",
		"product": "SAMSUNG",
		"variant": "SB-MFC",
		"vendor": "Suwon",
	},
	"build": {
		"path": "battery/suwon/wireless/sb_mfc",
		"file": "sb_mfc.py",
		"location": [
            {
                "dst": "drivers/battery/wireless/",
                "src": "*.c Kconfig:update Makefile:update",
            },
            {
                "dst": "include/linux/battery/",
                "src": "*.h",
            },
            {
                "dst": "arch/arm64/boot/dts/samsung/",
                "src": "dts/sb_mfc.dtsi",
            },
		],
	},
	"features": [
        {
            "configs": {
                "False": [
                    	"CONFIG_SB_MFC=y",
                ],
                "True": [
                    	"CONFIG_SB_MFC=m",
                ],
            },
            "label": "GKI_MODULE_BUILD",
            "type": "boolean",
            "value": True,
        },
        {
            "configs": {
                "False": [],
                "True": [],
            },
            "label": "General",
            "type": "boolean",
            "value": True,
        },
	],
    "interfaces": {
        "inports": [
            {
                "labels": [
                    {
                        "label": "GPIO: WPC_DET",
                        "parent": "${wpc_det_parent}",
                        "properties": [],
                        "symbol": "${mfc_det_gpio}",
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
        ]
    },
    "kunit_test": {
        "build": {
            "location": [
                {
                    "dst": "drivers/battery/wireless/kunit_test/",
                    "src": "kunit_test/*.c kunit_test/*.h kunit_test/Makefile",
                },
				{
					"dst": "include/linux/battery/",
					"src": "../../core/*.h",
				},
            ],
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
				"label": "test",
				"configs": {
					"False": [],
					"True": [
						"CONFIG_OF=y",
						"CONFIG_IIO=y",
						"CONFIG_I2C=y",
						"CONFIG_SB_MFC=y",
					],
				},
				"type": "boolean",
            },
	    ]
    },
}


def load():
    return config

if __name__ == '__main__':
    print(load())
