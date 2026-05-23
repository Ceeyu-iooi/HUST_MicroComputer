# 2026-05-22T22:14:30.737802
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_noInt")
status = platform.build()

comp = client.get_component(name="Task_noint")
comp.build()

status = platform.build()

comp.build()

platform = client.get_component(name="CPU_INT")
status = platform.build()

comp = client.get_component(name="CPU_qq")
comp.build()

vitis.dispose()

