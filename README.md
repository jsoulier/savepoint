# Savepoint

Savepoint is a lightweight ORM-style serialization framework for C++ applications.
Inspired by [cereal](https://github.com/USCiLab/cereal) and built on databases like [SQLite](https://sqlite.org/), it provides a simple interface for saving and loading C++ objects.

### Features

- Automatic transactions and schema upgrades
- Entity IDs and 2D/3D spatial keys
- Inherited, nested, polymorphic members and types
- Smart pointers, containers, tuples, random generators, etc
- Portable saves across platforms
- Opt-in thread safety and concurrent SQLite connections
- Backtraces
- Integrated visual debugger

### Examples

You can find all examples [here](examples)

```c++
#include <savepoint/savepoint.hpp>

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct Entity : SavepointEntity
{
    std::string Name;
    std::vector<int> Inventory;
    std::optional<float> Health;

    Entity() = default;
    Entity(std::string name, std::vector<int> inventory, std::optional<float> health)
        : Name{std::move(name)}
        , Inventory{std::move(inventory)}
        , Health{health}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Name);
        visitor(Inventory);
        visitor(Health);
    }

    bool operator==(const Entity& other) const
    {
        return Name == other.Name && Inventory == other.Inventory && Health == other.Health;
    }
};

int main()
{
    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", SavepointVersion{});

    Entity inEntity{"Player", {1, 2, 3}, 0.5f};
    savepoint.Write(inEntity, 0);
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        assert(outEntity == inEntity);
    }, 0);

    savepoint.Close();
    return 0;
}
```

### CMake

You can clone and add the following to your `CMakeLists.txt`:

```cmake
add_subdirectory(<path>)
target_link_libraries(<name> PRIVATE savepoint::savepoint)
```

### Documentation

The source contains Doxygen-style comments.
You can generate HTML docs with:

```shell
doxygen Doxyfile
```

### Debugger

To use the debugger, enable `SAVEPOINT_DEBUGGER` in CMake and integrate [imgui.hpp](include/savepoint/imgui.hpp) into your application

![](doc/image1.png)
![](doc/image2.png)
> Integrated into the [Asteroids](examples/13_asteroids.cpp) example
