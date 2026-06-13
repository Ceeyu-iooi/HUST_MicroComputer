# 2026-06-12T19:26:39.386445500
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

platform = client.get_component(name="CPU_UART")
status = platform.build()

comp = client.get_component(name="UART")
comp.build()

vitis.dispose()

