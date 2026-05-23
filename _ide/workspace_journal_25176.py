# 2026-05-12T21:48:39.797359400
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="platform")
status = platform.build()

status = platform.build()

comp = client.get_component(name="led")
comp.build()

status = platform.build()

comp.build()

client.delete_component(name="hello_world")

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

status = platform.build()

comp.build()

vitis.dispose()

