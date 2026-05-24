# 2026-05-24T13:10:52.886080600
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

comp = client.create_app_component(name="TASK_FAST_INT",platform = "$COMPONENT_LOCATION/../CPU_INT_TIMER/export/CPU_INT_TIMER/CPU_INT_TIMER.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_INT_0")
comp.build()

