# Code Standards Workflow

## Environment Variables
Always use the Kconfig.projbuild to save comopnent specific configuration variables and reference them with a definition variable.

Example:

```text
#define BROKER_URI CONFIG_MQTT_BROKER_URI
```
