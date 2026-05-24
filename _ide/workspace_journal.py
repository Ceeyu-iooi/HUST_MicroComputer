# 2026-05-24T21:30:41.807888500
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_FAST_INT")
comp.build()

status = platform.build()

comp.build()

