# 2026-05-22T20:56:37.276199100
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

comp = client.create_app_component(name="app_component",platform = "$COMPONENT_LOCATION/../CPU_INT/export/CPU_INT/CPU_INT.xpfm",domain = "standalone_microblaze_0")

comp = client.create_app_component(name="hello_world",platform = "$COMPONENT_LOCATION/../CPU_INT/export/CPU_INT/CPU_INT.xpfm",domain = "standalone_microblaze_0",template = "hello_world")

platform = client.get_component(name="CPU_INT")
status = platform.build()

comp = client.get_component(name="hello_world")
comp.build()

platform = client.create_platform_component(name = "CPU_noInt",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_noInt_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

client.delete_component(name="app_component")

comp = client.create_app_component(name="Task_noint",platform = "$COMPONENT_LOCATION/../CPU_noInt/export/CPU_noInt/CPU_noInt.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_noInt")
status = platform.build()

status = platform.build()

comp = client.get_component(name="Task_noint")
comp.build()

status = platform.build()

comp.build()

vitis.dispose()

