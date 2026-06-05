# 2026-05-29T19:03:06.151338600
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

platform = client.get_component(name="CPU_INT_TIMER")
status = platform.build()

comp = client.get_component(name="TASK_FAST_INT")
comp.build()

vitis.dispose()

