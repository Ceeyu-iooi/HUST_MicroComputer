# 2026-05-22T23:12:51.657910400
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_INT")
status = platform.build()

comp = client.get_component(name="CPU_qq")
comp.build()

platform = client.get_component(name="CPU_noInt")
status = platform.build()

status = platform.build()

comp = client.get_component(name="Task_noint")
comp.build()

vitis.dispose()

