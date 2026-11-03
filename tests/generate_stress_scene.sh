#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <sphere-count>" >&2
    exit 2
fi

count="$1"

case "${count}" in
''|*[!0-9]*)
    echo "sphere count must be a positive integer" >&2
    exit 2
    ;;
esac

if [ "${count}" -le 0 ]; then
    echo "sphere count must be a positive integer" >&2
    exit 2
fi

awk -v count="${count}" 'BEGIN {
    columns = int(sqrt(count))
    if (columns * columns < count)
        columns++
    rows = int((count + columns - 1) / columns)
    spacing = 1.15
    camera_height = rows * spacing * 0.75 + 3.0
    camera_distance = rows * spacing * 1.1 + 6.0

    print "camera:"
    print "{"
    printf "    position = { x = 0.0; y = %.2f; z = -%.2f; };\n", camera_height, camera_distance
    print "    rotation = { x = 0.0; y = 0.0; z = 0.0; };"
    print "    fieldOfView = 55.0;"
    print "};"
    print ""
    print "background:"
    print "{"
    print "    zenith = { r = 0.02; g = 0.03; b = 0.07; };"
    print "    horizon = { r = 0.10; g = 0.14; b = 0.24; };"
    print "    ground = { r = 0.01; g = 0.01; b = 0.02; };"
    print "    intensity = 1.0;"
    print "};"
    print ""
    print "materials:"
    print "{"
    print "    floor = {"
    print "        type = \"diffuse\";"
    print "        ambientReflection = 0.30;"
    print "        diffuseReflection = 0.85;"
    print "        color = { r = 0.20; g = 0.22; b = 0.28; };"
    print "    };"
    print "    a = {"
    print "        type = \"diffuse\";"
    print "        ambientReflection = 0.25;"
    print "        diffuseReflection = 0.90;"
    print "        color = { r = 0.55; g = 0.24; b = 0.18; };"
    print "    };"
    print "    b = {"
    print "        type = \"metallic\";"
    print "        roughness = 0.15;"
    print "        color = { r = 0.35; g = 0.65; b = 0.85; };"
    print "    };"
    print "    c = {"
    print "        type = \"diffuse\";"
    print "        ambientReflection = 0.25;"
    print "        diffuseReflection = 0.90;"
    print "        color = { r = 0.75; g = 0.70; b = 0.42; };"
    print "    };"
    print "};"
    print ""
    print "primitives:"
    print "{"
    print "    planes = ("
    print "        {"
    print "            material = 0;"
    print "            position = { x = 0.0; y = -0.5; z = 0.0; };"
    print "            normal = { x = 0.0; y = 1.0; z = 0.0; };"
    print "        }"
    print "    );"
    print ""
    print "    spheres = ("

    for (i = 0; i < count; i++) {
        column = i % columns
        row = int(i / columns)
        x = (column - (columns - 1) / 2.0) * spacing
        z = (row - (rows - 1) / 2.0) * spacing
        y = 0.15 + (i % 3) * 0.12
        material = 1 + (i % 3)

        if (i > 0)
            print "        },"
        print "        {"
        printf "            material = %d;\n", material
        print "            r = 0.38;"
        printf "            position = { x = %.3f; y = %.3f; z = %.3f; };\n", x, y, z
    }

    if (count > 0) {
        print "        }"
    }

    print "    );"
    print "};"
    print ""
    print "lights:"
    print "{"
    print "    ambient = {"
    print "        color = { r = 0.45; g = 0.50; b = 0.65; };"
    print "        intensity = 0.35;"
    print "    };"
    print "    directional = {"
    print "        position = { x = -0.35; y = -1.0; z = 0.45; };"
    print "        color = { r = 1.0; g = 0.96; b = 0.90; };"
    print "        intensity = 0.90;"
    print "    };"
    print "};"
}'
