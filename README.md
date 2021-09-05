
# Expmac

Expmac is utility to expand a particular macro in c/c++ source files.

# Example replacements.ini

```ini
[]
macro = CAGE_COMPONENT_ENGINE
params = (T,C,E)
value = T$$Component &C = E->value<T$$Component>();

[]
macro = CAGE_COMPONENT_GUI
params = (T,C,E)
value = Gui$$T$$Component &C = E->value<Gui$$T$$Component>();
```

Notice that all `#` are replaced with `$`, because `#` are used as comments in the file.

# Another example

```ini
[]
macro = real
value = Real
[]
macro = rads
value = Rads
[]
macro = degs
value = Degs
[]
macro = vec2
value = Vec2
[]
macro = vec3
value = Vec3
[]
macro = vec4
value = Vec4
[]
macro = ivec2
value = Vec2i
[]
macro = ivec3
value = Vec3i
[]
macro = ivec4
value = Vec4i
[]
macro = quat
value = Quat
[]
macro = mat3
value = Mat3
[]
macro = mat4
value = Mat4
[]
macro = transform
value = Transform
[]
macro = stringizer
value = Stringizer
[]
macro = string
value = String
```

# Building

See [BUILDING](https://github.com/ucpu/cage/blob/master/BUILDING.md) instructions for the Cage. They are the same here.
