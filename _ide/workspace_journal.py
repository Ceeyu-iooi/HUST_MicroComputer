# 2026-06-12T20:01:27.000937600
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.create_platform_component(name = "CPU_SPI",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_SPI_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="Sign",platform = "$COMPONENT_LOCATION/../CPU_SPI/export/CPU_SPI/CPU_SPI.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_SPI")
status = platform.build()

status = platform.build()

comp = client.get_component(name="Sign")
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

comp = client.create_app_component(name="example",platform = "$COMPONENT_LOCATION/../CPU_SPI/export/CPU_SPI/CPU_SPI.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="example")
comp.build()

client.delete_component(name="example")

client.delete_component(name="Sign")

comp = client.create_app_component(name="signal",platform = "$COMPONENT_LOCATION/../CPU_SPI/export/CPU_SPI/CPU_SPI.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="signal")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

client.delete_component(name="signal")

comp = client.create_app_component(name="Test",platform = "$COMPONENT_LOCATION/../CPU_INT_TIMER/export/CPU_INT_TIMER/CPU_INT_TIMER.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="Test")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

vitis.dispose()

