# 2026-05-24T13:05:51.857465800
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="TASK_INT")

vitis.dispose()

