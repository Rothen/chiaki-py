#ifndef CHIAKI_PY_STRUCT_WRAPPER_H
#define CHIAKI_PY_STRUCT_WRAPPER_H

template <typename T>
class StructWrapper
{
private:
    T wrapped_struct;
public:
    StructWrapper() : wrapped_struct{} {}
    StructWrapper(const T &wrapped) : wrapped_struct(wrapped) {}
    StructWrapper(T &&wrapped) : wrapped_struct(std::move(wrapped)) {}
    T *wrapped_ptr() { return &wrapped_struct; }
    T &wrapped() { return wrapped_struct; }
};

#endif // CHIAKI_PY_STRUCT_WRAPPER_H