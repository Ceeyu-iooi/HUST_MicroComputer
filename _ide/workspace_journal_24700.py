# 2026-05-23T09:36:49.517250600
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

comp = client.create_app_component(name="hello_world1",platform = "$COMPONENT_LOCATION/../CPU_noInt/export/CPU_noInt/CPU_noInt.xpfm",domain = "standalone_microblaze_0",template = "hello_world")

platform = client.get_component(name="CPU_noInt")
status = platform.build()

comp = client.get_component(name="hello_world1")
comp.build()

vitis.dispose()

