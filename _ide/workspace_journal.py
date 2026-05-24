# 2026-05-24T10:27:06.955603200
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_INT_0")
comp.build()

status = platform.build()

comp.build()

