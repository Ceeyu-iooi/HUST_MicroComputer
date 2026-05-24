# 2026-05-24T12:37:27.140996600
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="hello_world1")

client.delete_component(name="hello_world")

platform = client.get_component(name="CPU_noint")
status = platform.build()

comp = client.get_component(name="TASK_NOINT_")
comp.build()

vitis.dispose()

