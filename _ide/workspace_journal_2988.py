# 2026-05-13T12:29:33.995562900
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="led")

client.delete_component(name="platform")

platform = client.create_platform_component(name = "platform",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_Int/CPU_Int_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

platform = client.get_component(name="platform")
status = platform.build()

comp = client.create_app_component(name="app",platform = "$COMPONENT_LOCATION/../platform/export/platform/platform.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

status = platform.build()

comp = client.get_component(name="app")
comp.build()

status = platform.build()

comp.build()

status = platform.build()

status = platform.build()

comp.build()

vitis.dispose()

