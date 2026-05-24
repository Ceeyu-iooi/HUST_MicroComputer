# 2026-05-23T21:46:37.486372100
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_INT_0")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

platform = client.create_platform_component(name = "CPU_INT_TIMER_1",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_INT_1_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="TASK_INT_EDGE",platform = "$COMPONENT_LOCATION/../CPU_INT_TIMER_1/export/CPU_INT_TIMER_1/CPU_INT_TIMER_1.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_INT_TIMER_1")
status = platform.build()

comp = client.get_component(name="TASK_INT_EDGE")
comp.build()

vitis.dispose()

