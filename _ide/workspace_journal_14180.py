# 2026-05-23T10:00:24.275571
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="CPU_noInt")

platform = client.create_platform_component(name = "CPU_noint",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_noInt/CPU_noint_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

platform = client.get_component(name="CPU_noint")
status = platform.build()

status = platform.build()

comp = client.get_component(name="Task_noint")
comp.build()

comp = client.create_app_component(name="TASK_NOINT_",platform = "$COMPONENT_LOCATION/../CPU_noint/export/CPU_noint/CPU_noint.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="TASK_NOINT_")
comp.build()

client.delete_component(name="Task_noint")

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

