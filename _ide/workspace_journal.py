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

