# 2026-05-23T12:08:48.391973200
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_noint")
status = platform.build()

comp = client.get_component(name="TASK_NOINT_")
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

comp = client.create_app_component(name="TASK_INT",platform = "$COMPONENT_LOCATION/../CPU_noint/export/CPU_noint/CPU_noint.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="TASK_INT")
comp.build()

status = platform.build()

comp.build()

platform = client.create_platform_component(name = "CPU_INT_TIMER",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_INT_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="TASK_INT_0",platform = "$COMPONENT_LOCATION/../CPU_INT_TIMER/export/CPU_INT_TIMER/CPU_INT_TIMER.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_INT_0")
comp.build()

vitis.dispose()

