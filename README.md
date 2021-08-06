
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

# Building

See [BUILDING](https://github.com/ucpu/cage/blob/master/BUILDING.md) instructions for the Cage. They are the same here.
