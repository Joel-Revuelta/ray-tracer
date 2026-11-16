/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** DielectricStack.hpp
*/

#ifndef DIELECTRIC_STACK_HPP_
#define DIELECTRIC_STACK_HPP_

#if defined(__CUDACC__)
#define RAYTRACER_HOST_DEVICE __host__ __device__
#else
#define RAYTRACER_HOST_DEVICE
#endif

namespace RayTracer
{

/**
 * Fixed-size stack of active dielectric interfaces for one camera ray.
 * `indices` identifies the hit primitive and `iors` stores the material IOR
 * entered at that interface. The top of the stack is the current medium.
 */
struct DielectricStack
{
    static constexpr int MAX_INTERFACES = 16;

    int indices[MAX_INTERFACES];
    float iors[MAX_INTERFACES];
    int size;
};

RAYTRACER_HOST_DEVICE inline void dielectricReset(DielectricStack &stack)
{
    stack.size = 0;
}

RAYTRACER_HOST_DEVICE inline int dielectricFind(const DielectricStack &stack, int index)
{
    for (int i = 0; i < stack.size; ++i)
    {
        if (stack.indices[i] == index)
            return i;
    }
    return -1;
}

/** IOR of the current medium, or air when the ray is outside all dielectrics. */
RAYTRACER_HOST_DEVICE inline float dielectricSurroundingIor(const DielectricStack &stack)
{
    return stack.size == 0 ? 1.0f : stack.iors[stack.size - 1];
}

/** IOR outside the interface at `position`, assuming properly nested media. */
RAYTRACER_HOST_DEVICE inline float dielectricExteriorIor(const DielectricStack &stack, int position)
{
    return position <= 0 ? 1.0f : stack.iors[position - 1];
}

RAYTRACER_HOST_DEVICE inline bool dielectricPush(DielectricStack &stack, int index, float ior)
{
    if (stack.size >= DielectricStack::MAX_INTERFACES)
        return false;
    if (dielectricFind(stack, index) >= 0)
        return false;

    stack.indices[stack.size] = index;
    stack.iors[stack.size] = ior;
    ++stack.size;
    return true;
}

RAYTRACER_HOST_DEVICE inline void dielectricRemove(DielectricStack &stack, int index)
{
    for (int i = stack.size - 1; i >= 0; --i)
    {
        if (stack.indices[i] != index)
            continue;

        for (int j = i; j + 1 < stack.size; ++j)
        {
            stack.indices[j] = stack.indices[j + 1];
            stack.iors[j] = stack.iors[j + 1];
        }
        --stack.size;
        return;
    }
}

} // namespace RayTracer

#endif // DIELECTRIC_STACK_HPP_
