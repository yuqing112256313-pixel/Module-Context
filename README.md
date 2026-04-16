# Module Context Core

`module_context_project_cmake` is a small module runtime framework built on top
of `foundation_0415_v1`.

## What Changed

- Dynamic-library loading and plugin lifetime now delegate to
  `foundation::plugin::PluginLoader<IModule>`.
- Config-file loading now uses `foundation::config::ConfigReader`.
- Public lifecycle and loading APIs now return
  `foundation::base::Result<void>` instead of `bool`/`void`.
- Export macros now follow the same platform split as `foundation`.
- Common infrastructure such as `NonCopyable`, `Assert`, path normalization, and
  error codes now come from `foundation`.
- Module export symbols now follow foundation's plugin convention:
  `GetPluginApiVersion`, `CreatePlugin`, `DestroyPlugin`.

## Build

Dependency resolution order:
- Reuse `foundation::foundation` if a parent project already provides it.
- Use `MC_FOUNDATION_SOURCE_DIR` if you point it at a local checkout.
- Fall back to common sibling checkouts such as `../foundation_0415_v1/foundation`
  or `../Foundation`.
- Otherwise fetch `foundation` automatically from
  `https://github.com/yuqing112256313-pixel/Foundation.git`.

Build with:

```bash
cmake -S . -B build
cmake --build build -j
```

To force a specific local checkout:

```bash
cmake -S . -B build -DMC_FOUNDATION_SOURCE_DIR=/path/to/Foundation
```

## Usage

```cpp
#include "core/framework/Context.h"
#include "foundation/base/ErrorCode.h"

module_context::framework::Context context;

foundation::base::Result<void> result = context.Init();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Start();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Stop();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}

result = context.Fini();
if (!result.IsOk()) {
    return static_cast<int>(result.GetError());
}
```

The loader now accepts one canonical JSON module config format.

Recommended standard:
- The root must be a JSON object.
- `schema_version` is required and must currently be `1`.
- `modules` is required and must be an array.
- Module lifecycle order is exactly the order of the `modules` array.
- Each module entry must contain non-empty string fields `name` and
  `library_path`.
- Module names must be unique in one config file.
- Relative `library_path` values are resolved against the config file
  directory.

```json
{
  "schema_version": 1,
  "modules": [
    {
      "name": "MyModule",
      "library_path": "./plugins/MyModule.dll"
    },
    {
      "name": "OtherModule",
      "library_path": "./plugins/OtherModule.dll"
    }
  ]
}
```

Plugin modules should export the factory functions via:

```cpp
MC_DECLARE_MODULE_FACTORY(YourModuleType)
```

This now emits foundation-compatible plugin symbols and API version metadata.

## Runtime Behavior

- Module lifecycle order remains `Init -> Start -> Stop -> Fini`.
- `LoadModules()` validates the whole JSON config before committing loaded
  modules into the manager.
- `Stop()` and `Fini()` run in reverse module order.
- Loading stops on the first failure and returns a `foundation` error code plus
  message.
- `Module(name)` now returns `foundation::base::Result<IModule*>`.
