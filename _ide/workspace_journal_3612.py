# 2026-06-05T20:58:52.469741600
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.create_platform_component(name = "CPU_UART",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_UART_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="UART",platform = "$COMPONENT_LOCATION/../CPU_UART/export/CPU_UART/CPU_UART.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_UART")
status = platform.build()

comp = client.get_component(name="UART")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

vitis.dispose()

