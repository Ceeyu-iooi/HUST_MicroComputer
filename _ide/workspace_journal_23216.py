# 2026-05-11T23:08:16.749284500
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.create_platform_component(name = "platform",hw_design = "$COMPONENT_LOCATION/../../../Projects/CPU_Int/CPU_Int_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="app_component",platform = "$COMPONENT_LOCATION/../platform/export/platform/platform.xpfm",domain = "standalone_microblaze_0")

comp = client.create_app_component(name="CPU_Int",platform = "$COMPONENT_LOCATION/../platform/export/platform/platform.xpfm",domain = "standalone_microblaze_0")

comp = client.clone_component(name="app_component",new_name="obob")

client.delete_component(name="app_component")

client.delete_component(name="obob")

platform = client.get_component(name="platform")
status = platform.build()

status = platform.build()

comp = client.get_component(name="CPU_Int")
comp.build()

status = platform.build()

comp.build()

vitis.dispose()

