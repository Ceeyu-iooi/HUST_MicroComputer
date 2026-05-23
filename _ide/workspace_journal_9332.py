# 2026-05-12T10:39:55.346339200
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="CPU_Int")

comp = client.create_app_component(name="helloworld",platform = "$COMPONENT_LOCATION/../platform/export/platform/platform.xpfm",domain = "standalone_microblaze_0")

comp = client.create_app_component(name="hello_world",platform = "$COMPONENT_LOCATION/../platform/export/platform/platform.xpfm",domain = "standalone_microblaze_0",template = "hello_world")

client.delete_component(name="helloworld")

platform = client.get_component(name="platform")
status = platform.build()

status = platform.build()

comp = client.get_component(name="hello_world")
comp.build()

vitis.dispose()

