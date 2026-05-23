### shared
This directory contains code shared by both the `runtime` and a `project`.
- Some common macros like `DISABLE_COPY(T)`
- `exitWithError()`
- `isApple` boolean flag
- General size types: `Size2D`, `Size3D`
- Type and function aliases (`ptr.hpp`, `result.hpp`)

  | `std`                   | Alias             |
  |-------------------------|-------------------|
  | `std::unique_ptr<T>`    | `UPtr<T>`         |
  | `std::make_unique<T>()` | `makeUnique<T>()` |
  | `std::shared_ptr<T>`    | `SPtr<T>`         |
  | `std::make_shared<T>()` | `makeShared<T>()` |
  | `std::expected<T, E>`   | `Result<T>`       |
| | `std::optional<T>`      | `Option<T>`       |

- Convenience methods taking Ranges as input:

  | Function       | Description                                                                                                                           |
  |----------------|---------------------------------------------------------------------------------------------------------------------------------------|
  | `join()`       | Convert the elements of a range to string via `<<` and join them with the specified delimiter.                                        |
  | `contains()`   | Utility method for `std::ranges::find(...) != std::ranges::end(...)`.                                                                 |
  | `containsIf()` | `contains()` but with a lambda as predicate.                                                                                          |
  | `enumerate()`  | Implements a basic version of the missing `std::views::enumerate()` from clang libc++. Otherwise it uses the standard implementation. |
