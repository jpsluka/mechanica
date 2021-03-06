/*******************************************************************************
 * This file is part of mdcore.
 * Coypright (c) 2010 Pedro Gonnet (pedro.gonnet@durham.ac.uk)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 ******************************************************************************/


/* include some standard header files */
#include <stdlib.h>
#include <math.h>
#include <MxParticle.h>
#include "fptype.h"
#include <iostream>

// python type info
#include <structmember.h>
#include <MxNumpy.h>

#include <pybind11/pybind11.h>

#include <MxPy.h>
#include "engine.h"
#include "space.h"
#include "mx_runtime.h"




struct Foo {
    int x; int y; int z;
};


//template <typename C, typename D, typename... Extra>
//class_ &def_readwrite(const char *name, D C::*pm, const Extra&... extra) {


    
    
template<typename C, typename T>
void f(T C::*pm)
{
    std::cout << "sizeof pm: " << sizeof(pm) << std::endl;
    std::cout << "sizeof T: " << sizeof(T) << std::endl;
    //std::cout << "sizeof *pm: " << sizeof(MxParticle::*pm) << std::endl;
    std::cout << typeid(T).name() << std::endl;
    
    if(std::is_same<T, float>::value) {
        std::cout << "is float" << std::endl;
    }
    
    if(std::is_same<T, Magnum::Vector3>::value) {
        std::cout << "is vec" << std::endl;
    }
    
    std::cout << "offset of: " << offset_of(pm);
}

/**
 * initialize a newly allocated type
 *
 * adds a new data entry to the engine.
 */
static HRESULT MxParticleType_Init(MxParticleType *self, PyObject *dict);

static void printTypeInfo(const char* name, PyTypeObject *p);


 



static PyObject *particle_getattro(PyObject* obj, PyObject *name) {
    
    PyObject *s = PyObject_Str(name);
    PyObject* pyStr = PyUnicode_AsEncodedString(s, "utf-8", "Error ~");
    const char *cstr = PyBytes_AS_STRING(pyStr);
    std::cout << obj->ob_type->tp_name << ": " << __PRETTY_FUNCTION__ << ":" << cstr << "\n";
    return PyObject_GenericGetAttr(obj, name);
}






struct Offset {
    uint32_t kind;
    uint32_t offset;
};

static_assert(sizeof(Offset) == sizeof(void*), "error, void* must be 64 bit");

static_assert(sizeof(MxGetSetDefInfo) == sizeof(void*), "error, void* must be 64 bit");
static_assert(sizeof(MxGetSetDef) == sizeof(PyGetSetDef), "error, void* must be 64 bit");

PyObject * vector4_getter(MxParticle *obj, void *closure) {
    void* pClosure = &closure;
    Offset o = *(Offset*)pClosure;

    char* pVec = ((char*)obj) + o.offset;

    Magnum::Vector4 *vec = (Magnum::Vector4 *)pVec;

    pybind11::handle h = pybind11::cast(vec).release();
    
    //pybind11::object h2 = pybind11::cast(*vec);
    

    
    PyObject *result = h.ptr();
    
    std::cout << "result: " << result << std::endl;
    std::cout << "result.refcnt: " << result->ob_refcnt << std::endl;
    std::cout << "result.type: " << result->ob_type->tp_name << std::endl;

    return result;

}

int vector4_setter(PyObject *, PyObject *, void *) {

    return -1;

}



::PyGetSetDef create_vector4_getset() {

    ::PyGetSetDef result;

    result.name = "foo";
    result.get = (getter)vector4_getter;
    result.set = vector4_setter;
    result.doc = "docs";

    Offset o = {0, offsetof(MxParticle, position)};

    void** p = (void**)&o;


    result.closure = (void*)(*p);


    return result;

}


PyGetSetDef gsd = {
        .name = "descr",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            const char* on = obj != NULL ? obj->ob_type->tp_name : "NULL";
            std::cout << "getter(obj.type:" << on << ", p:" << p << ")" << std::endl;

            bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
            bool isParticleType = PyObject_IsInstance(obj, (PyObject*)&MxParticleType_Type);

            std::cout << "is particle: " << isParticle << std::endl;
            std::cout << "is particle type: " << isParticleType << std::endl;
            return PyLong_FromLong(567);
        },
        .set = [](PyObject *obj, PyObject *, void *p) -> int {
            const char* on = obj != NULL ? obj->ob_type->tp_name : "NULL";
            std::cout << "setter(obj.type:" << on << ", p:" << p << ")" << std::endl;

            bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
            bool isParticleType = PyObject_IsInstance(obj, (PyObject*)&MxParticleType_Type);

            std::cout << "is particle: " << isParticle << std::endl;
            std::cout << "is particle type: " << isParticleType << std::endl;

            return 0;
        },
        .doc = "test doc",
        .closure = NULL
    };

PyGetSetDef gs_charge = {
    .name = "charge",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->charge).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        
        try {
            double *x = &type->charge;
            *x = pybind11::cast<double>(val);
            return 0;
        }
        catch (const pybind11::builtin_exception &e) {
            e.set_error();
            return -1;
        }
    },
    .doc = "test doc",
    .closure = NULL
};

PyGetSetDef gs_mass = {
    .name = "mass",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->mass).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        
        try {
            double *x = &type->mass;
            *x = pybind11::cast<double>(val);
            return 0;
        }
        catch (const pybind11::builtin_exception &e) {
            e.set_error();
            return -1;
        }
    },
    .doc = "test doc",
    .closure = NULL
};

PyGetSetDef gs_name = {
    .name = "name",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->name).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
        PyErr_SetString(PyExc_PermissionError, "read only");
        return -1;
    },
    .doc = "test doc",
    .closure = NULL
};

PyGetSetDef gs_name2 = {
    .name = "name2",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        bool isParticle = PyObject_IsInstance(obj, (PyObject*)MxParticle_GetType());
        MxParticleType *type = NULL;
        if(isParticle) {
            type = (MxParticleType*)obj->ob_type;
        }
        else {
            type = (MxParticleType*)obj;
        }
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->name2).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
        PyErr_SetString(PyExc_PermissionError, "read only");
        return -1;
    },
    .doc = "test doc",
    .closure = NULL
};

// temperature is an ensemble property
PyGetSetDef gs_type_temperature = {
    .name = "temperature",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        MxParticleType *type = type = (MxParticleType*)obj;
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->kinetic_energy).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            return -1;
    },
    .doc = "test doc",
    .closure = NULL
};

PyGetSetDef gs_type_target_temperature = {
    .name = "target_temperature",
    .get = [](PyObject *obj, void *p) -> PyObject* {
        MxParticleType *type = (MxParticleType*)obj;
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        return pybind11::cast(type->target_energy).release().ptr();
    },
    .set = [](PyObject *obj, PyObject *val, void *p) -> int {
        MxParticleType *type = (MxParticleType*)obj;
        assert(type && PyObject_IsInstance((PyObject*)type, (PyObject*)&MxParticleType_Type));
        
        try {
            double *x = &type->target_energy;
            *x = pybind11::cast<double>(val);
            return 0;
        }
        catch (const pybind11::builtin_exception &e) {
            e.set_error();
            return -1;
        }
    },
    .doc = "test doc",
    .closure = NULL
};



PyGetSetDef particle_getsets[] = {
    gs_charge,
    gs_mass,
    gs_name,
    gs_name2,
    gsd,
    {
        .name = "position",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            int id = ((MxPyParticle*)obj)->part->id;
            Magnum::Vector3 vec;
            space_getpos(&_Engine.s, id, vec.data());
            return pybind11::cast(vec).release().ptr();
            
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                int id = ((MxPyParticle*)obj)->part->id;
                Magnum::Vector3 vec = pybind11::cast<Magnum::Vector3>(val);
                space_setpos(&_Engine.s, id, vec.data());
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {
        .name = "velocity",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            Magnum::Vector3 *vec = &((MxPyParticle*)obj)->part->velocity;
            return pybind11::cast(vec).release().ptr();
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                Magnum::Vector3 *vec = &((MxPyParticle*)obj)->part->velocity;
                *vec = pybind11::cast<Magnum::Vector3>(val);
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {
        .name = "force",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            Magnum::Vector3 *vec = &((MxPyParticle*)obj)->part->force;
            return pybind11::cast(vec).release().ptr();
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                Magnum::Vector3 *vec = &((MxPyParticle*)obj)->part->force;
                *vec = pybind11::cast<Magnum::Vector3>(val);
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {
        .name = "id",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            int x = ((MxPyParticle*)obj)->part->id;
            return pybind11::cast(x).release().ptr();
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                int *x = &((MxPyParticle*)obj)->part->id;
                *x = pybind11::cast<int>(val);
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {
        .name = "type_id",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            int x = ((MxPyParticle*)obj)->part->typeId;
            return pybind11::cast(x).release().ptr();
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                short *x = &((MxPyParticle*)obj)->part->typeId;
                *x = pybind11::cast<short>(val);
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {
        .name = "flags",
        .get = [](PyObject *obj, void *p) -> PyObject* {
            unsigned short x = ((MxPyParticle*)obj)->part->flags;
            return pybind11::cast(x).release().ptr();
        },
        .set = [](PyObject *obj, PyObject *val, void *p) -> int {
            try {
                unsigned short *x = &((MxPyParticle*)obj)->part->flags;
                *x = pybind11::cast<unsigned short>(val);
                return 0;
            }
            catch (const pybind11::builtin_exception &e) {
                e.set_error();
                return -1;
            }
        },
        .doc = "test doc",
        .closure = NULL
    },
    {NULL}
};

static PyObject* particle_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    //std::cout << MX_FUNCTION << ", type: " << type->tp_name << std::endl;
    return PyType_GenericNew(type, args, kwargs);
}

static int particle_init(MxPyParticle *self, PyObject *_args, PyObject *_kwds) {
    // std::cout << MX_FUNCTION << std::endl;
    
    MxParticleType *type = (MxParticleType*)self->ob_type;
    
    MxParticle part = {
        .position = {},
        .velocity = {},
        .force = {},
        .typeId = type->id,
        .id = _Engine.s.nr_parts
    };
    
    
    try {
        pybind11::detail::loader_life_support ls{};
        pybind11::args args = pybind11::reinterpret_borrow<pybind11::args>(_args);
        pybind11::kwargs kwargs = pybind11::reinterpret_borrow<pybind11::kwargs>(_kwds);
        
        part.position = arg<Magnum::Vector3>("position", 0, args.ptr(), kwargs.ptr(), Magnum::Vector3{});
        part.velocity = arg<Magnum::Vector3>("velocity", 1, args.ptr(), kwargs.ptr(), Magnum::Vector3{});
        
        MxParticle *p = NULL;
        double pos[] = {part.position[0], part.position[1], part.position[2]};
        int result = engine_addpart (&_Engine, &part, pos, &p);
        
        self->part = p;
        
        return 0;
    }
    catch (const pybind11::builtin_exception &e) {
        e.set_error();
        return -1;
    }
}


/**
 * The base particle type
 * this instance points to the 0'th item in the global engine struct.
 *
MxParticleType MxParticle_Type = {
{
  {
      PyVarObject_HEAD_INIT(NULL, 0)
      .tp_name =           "Particle",
      .tp_basicsize =      sizeof(MxPyParticle),
      .tp_itemsize =       0, 
      .tp_dealloc =        0, 
      .tp_print =          0, 
      .tp_getattr =        0, 
      .tp_setattr =        0, 
      .tp_as_async =       0, 
      .tp_repr =           0, 
      .tp_as_number =      0, 
      .tp_as_sequence =    0, 
      .tp_as_mapping =     0, 
      .tp_hash =           0, 
      .tp_call =           0, 
      .tp_str =            0, 
      .tp_getattro =       0,
      .tp_setattro =       0, 
      .tp_as_buffer =      0, 
      .tp_flags =          Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_doc =            "Custom objects",
      .tp_traverse =       0, 
      .tp_clear =          0, 
      .tp_richcompare =    0, 
      .tp_weaklistoffset = 0, 
      .tp_iter =           0, 
      .tp_iternext =       0, 
      .tp_methods =        0, 
      .tp_members =        0,
      .tp_getset =         particle_getsets,
      .tp_base =           0, 
      .tp_dict =           0, 
      .tp_descr_get =      0, 
      .tp_descr_set =      0, 
      .tp_dictoffset =     0, 
      .tp_init =           (initproc)particle_init,
      .tp_alloc =          0, 
      .tp_new =            particle_new,
      .tp_free =           0, 
      .tp_is_gc =          0, 
      .tp_bases =          0, 
      .tp_mro =            0, 
      .tp_cache =          0, 
      .tp_subclasses =     0, 
      .tp_weaklist =       0, 
      .tp_del =            [] (PyObject *p) -> void {
          std::cout << "tp_del MxPyParticle" << std::endl;
      },
      .tp_version_tag =    0, 
      .tp_finalize =       [] (PyObject *p) -> void {
          // std::cout << "tp_finalize MxPyParticle" << std::endl;
      }
    }
  },
};
*/



static getattrofunc savedFunc = NULL;

static PyObject *particle_type_getattro(PyObject* obj, PyObject *name) {
    
    PyObject *s = PyObject_Str(name);
    PyObject* pyStr = PyUnicode_AsEncodedString(s, "utf-8", "Error ~");
    const char *cstr = PyBytes_AS_STRING(pyStr);
    //std::cout << obj->ob_type->tp_name << ": " << __PRETTY_FUNCTION__ << ":" << cstr << "\n";
    return savedFunc(obj, name);
}

/**
 * Basically a copy of Python's
 * PyType_GenericAlloc(PyTypeObject *type, Py_ssize_t nitems)
 *
 * Want to format the memory identically to Python, except we allocate
 * the object in the engine's static array of types.
 */
PyObject *particle_type_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
    printTypeInfo("particle_type_alloc", type);

    MxParticleType *obj;
    const size_t size = _PyObject_VAR_SIZE(type, nitems+1);
    /* note that we need to add one, for the sentinel */

    if (PyType_IS_GC(type)) {
        PyErr_SetString(PyExc_MemoryError, "Fatal error, particle type can not be a garbage collected type");
        return NULL;
    }
    else if(engine::nr_types >= engine::max_type) {
        PyErr_SetString(PyExc_MemoryError, "out of memory for new particle type");
        return NULL;
    }

    obj = &engine::types[engine::nr_types];
    memset(obj, '\0', size);
    obj->id = engine::nr_types;
    engine::nr_types++;

    if (type->tp_flags & Py_TPFLAGS_HEAPTYPE)
        Py_INCREF(type);

    if (type->tp_itemsize == 0)
        (void)PyObject_INIT(obj, type);
    else
        (void) PyObject_INIT_VAR((PyVarObject *)obj, type, nitems);

    return (PyObject*)obj;
}

static PyObject *
particle_type_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    std::string t = pybind11::cast<pybind11::str>((PyObject*)type);
    std::string a = pybind11::cast<pybind11::str>(args);
    std::string k = pybind11::cast<pybind11::str>(kwds);
    
    std::cout << MX_FUNCTION << "(type: " << t << ", args: " << a << ", kwargs: " << k << ")" << std::endl;
    
    PyTypeObject *result;
    PyObject *fields;

    /* create the new instance (which is a class,
           since we are a metatype!) */
    result = (PyTypeObject *)PyType_Type.tp_new(type, args, kwds);

    if (!result)
        return NULL;

    return (PyObject*)result;
}

/*
 *   ID of this type
    int id;

    /** Constant physical characteristics
    double mass, imass, charge;

    /** Nonbonded interaction parameters.
    double eps, rmin;

    /** Name of this paritcle type.
    char name[64], name2[64];
 */


static PyGetSetDef particle_type_getset[] = {
    gsd,
    gs_charge,
    gs_mass,
    gs_name,
    gs_name2,
    gs_type_temperature,
    gs_type_target_temperature,
    {NULL},
};

static PyObject *
particle_type_descr_get(PyMemberDescrObject *descr, PyObject *obj, PyObject *type)
{
    return PyType_Type.tp_descr_get((PyObject*)descr, obj, type);
}

static int particle_type_init(MxParticleType *self, PyObject *_args, PyObject *kwds) {
    
    std::string s = pybind11::cast<pybind11::str>((PyObject*)self);
    std::string a = pybind11::cast<pybind11::str>(_args);
    std::string k = pybind11::cast<pybind11::str>(kwds);
    
    std::cout << MX_FUNCTION << "(self: " << s << ", args: " << a << ", kwargs: " << k << ")" << std::endl;
    
    //args is a tuple of (name, (bases, .), dict),
    pybind11::tuple args = pybind11::reinterpret_borrow<pybind11::tuple>(_args);
    
    pybind11::str name = args[0];
    pybind11::tuple bases = args[1];
    pybind11::object dict = args[2];

    return MxParticleType_Init(self, dict.ptr());
}

/**
 * particle type metatype
 */
PyTypeObject MxParticleType_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name =           "ParticleType",
    .tp_basicsize =      sizeof(MxParticleType),
    .tp_itemsize =       0, 
    .tp_dealloc =        0, 
    .tp_print =          0, 
    .tp_getattr =        0, 
    .tp_setattr =        0, 
    .tp_as_async =       0, 
    .tp_repr =           0, 
    .tp_as_number =      0, 
    .tp_as_sequence =    0, 
    .tp_as_mapping =     0, 
    .tp_hash =           0, 
    .tp_call =           0, 
    .tp_str =            0, 
    .tp_getattro =       0, 
    .tp_setattro =       0, 
    .tp_as_buffer =      0, 
    .tp_flags =          Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc =            "Custom objects",
    .tp_traverse =       0, 
    .tp_clear =          0, 
    .tp_richcompare =    0, 
    .tp_weaklistoffset = 0, 
    .tp_iter =           0, 
    .tp_iternext =       0, 
    .tp_methods =        0, 
    .tp_members =        0,
    .tp_getset =         particle_type_getset,
    .tp_base =           0, 
    .tp_dict =           0, 
    .tp_descr_get =      (descrgetfunc)particle_type_descr_get,
    .tp_descr_set =      0, 
    .tp_dictoffset =     0, 
    .tp_init =           (initproc)particle_type_init,
    .tp_alloc =          particle_type_alloc, 
    .tp_new =            particle_type_new,
    .tp_free =           0, 
    .tp_is_gc =          0, 
    .tp_bases =          0, 
    .tp_mro =            0, 
    .tp_cache =          0, 
    .tp_subclasses =     0, 
    .tp_weaklist =       0, 
    .tp_del =            0, 
    .tp_version_tag =    0, 
    .tp_finalize =       0, 
};




/** ID of the last error */
int particle_err = PARTICLE_ERR_OK;



static void printTypeInfo(const char* name, PyTypeObject *p) {
    
    uint32_t is_gc = p->tp_flags & Py_TPFLAGS_HAVE_GC;

    
    std::cout << "type: {" << std::endl;
    std::cout << "  name: " << name << std::endl;
    std::cout << "  type_name: " << Py_TYPE(p)->tp_name << std::endl;
    std::cout << "  basetype_name:" << p->tp_base->tp_name << std::endl;
    std::cout << "  have gc: " << std::to_string((bool)PyType_IS_GC(p)) << std::endl;
    std::cout << "}" << std::endl;
    
    /*
    if(p->tp_getattro) {
        PyObject *o = PyUnicode_FromString("foo");
        p->tp_getattro((PyObject*)p, o);
    }
     */
}

HRESULT _MxParticle_init(PyObject *m)
{
    f<>(&MxParticle::q);
    
    f<>(&MxParticle::position);
    
    f<>(&MxParticle::x);
    
    uint32_t def = Py_TPFLAGS_DEFAULT & Py_TPFLAGS_HAVE_GC;
    
    uint32_t base = Py_TPFLAGS_BASETYPE & Py_TPFLAGS_HAVE_GC;
    
    std::cout << "default: " << def << ", basetype: " << base << std::endl;
    
    /*************************************************
     *
     * Metaclasses first
     */
    
   std::cout << "MxParticleType_Type have gc: " << std::to_string((bool)PyType_IS_GC(&MxParticleType_Type)) << std::endl;

    //PyCStructType_Type.tp_base = &PyType_Type;
    // if (PyType_Ready(&PyCStructType_Type) < 0)
    //     return NULL;
    MxParticleType_Type.tp_base = &PyType_Type;
    if (PyType_Ready((PyTypeObject*)&MxParticleType_Type) < 0) {
        std::cout << "could not initialize MxParticleType_Type " << std::endl;
        return E_FAIL;
    }
    
    // clear the GC of the particle type. PyTypeReady causes this to
    // inherit flags from the base type, PyType_Type. Because we
    // manage our own memory, clear these bits.
    MxParticleType_Type.tp_flags &= ~(Py_TPFLAGS_HAVE_GC);
    MxParticleType_Type.tp_clear = NULL;
    MxParticleType_Type.tp_traverse = NULL;
    
    printTypeInfo("MxParticleType_Type", &MxParticleType_Type);
    

    

    /*************************************************
     *
     * Classes using a custom metaclass second
     */
    // Py_TYPE(&Struct_Type) = &PyCStructType_Type;
    // Struct_Type.tp_base = &PyCData_Type;
    // if (PyType_Ready(&Struct_Type) < 0)
    //     return NULL;
    //Py_TYPE(&MxParticle_Type) = &MxParticleType_Type;
    //MxParticle_Type.tp_base = &PyBaseObject_Type;
    //if (PyType_Ready((PyTypeObject*)&MxParticle_Type) < 0) {
    //    std::cout << "could not initialize MxParticle_Type " << std::endl;
    //    return E_FAIL;
    //}
    
    if(MxParticleType_Type.tp_getattro) {
        savedFunc = MxParticleType_Type.tp_getattro;
        MxParticleType_Type.tp_getattro = particle_type_getattro;
    }
    

    Py_INCREF(&MxParticleType_Type);
    if (PyModule_AddObject(m, "ParticleType", (PyObject *)&MxParticleType_Type) < 0) {
        Py_DECREF(&MxParticleType_Type);
        return E_FAIL;
    }

    //Py_INCREF(&MxParticle_Type);
    //if (PyModule_AddObject(m, "Particle", (PyObject *)&MxParticle_Type) < 0) {
    //    Py_DECREF(&MxParticle_Type);
    //    return E_FAIL;
    //}

    return  engine_particle_base_init(m);;
}

int MxParticleCheck(PyObject *o)
{
    return -1;
}

MxPyParticle* MxPyParticle_New(MxParticle *data)
{
    PyTypeObject *type = (PyTypeObject*)&_Engine.types[data->typeId];
    MxPyParticle *part = (MxPyParticle*)PyType_GenericAlloc(type, 0);
    part->part = data;
    return part;
}


MxParticleType* MxParticleType_New(const char *_name, PyObject *dict)
{    
    pybind11::str name(_name);
    pybind11::tuple bases(1);
    bases[0] = (PyObject*)MxParticle_GetType();
    pybind11::tuple args(3);
    args[0] = name;
    args[1] = bases;
    args[2] = dict;

    MxParticleType *result = (MxParticleType*)PyType_Type.tp_call((PyObject*)&PyType_Type, args.ptr(), NULL);

    assert(result && PyType_IsSubtype((PyTypeObject*)result, (PyTypeObject*)MxParticle_GetType()));

    return result;
}

HRESULT MxParticleType_Init(MxParticleType *self, PyObject *_dict)
{
    self->mass = 1.0;
    self->charge = 0.0;
    self->target_energy = 0.;

    std::strncpy(self->name, self->ht_type.tp_name, MxParticleType::MAX_NAME);
    
    try {
        pybind11::dict dict = pybind11::reinterpret_borrow<pybind11::dict>(_dict);
        if(dict.contains("mass")) {
            self->mass = dict["mass"].cast<double>();
        }
        
        if(dict.contains("target_temperature")) {
            self->target_energy = dict["target_temperature"].cast<double>();
        }

        self->imass = 1.0 / self->mass;

        if(dict.contains("charge")) {
            self->charge = dict["charge"].cast<double>();
        }
        
        if(dict.contains("name2")) {
            std::string name2 = dict["name2"].cast<std::string>();
            std::strncpy(self->name2, name2.c_str(), MxParticleType::MAX_NAME);
        }
        
        // pybind does not seem to wrap deleting item from dict, WTF?!?
        if(self->ht_type.tp_dict) {
            
            PyObject *_dict = self->ht_type.tp_dict;
            
            pybind11::object key = pybind11::cast("mass");
            if(PyDict_Contains(_dict, key.ptr())) {
                PyDict_DelItem(_dict, key.ptr());
            }
            key = pybind11::cast("charge");
            if(PyDict_Contains(_dict, key.ptr())) {
                PyDict_DelItem(_dict, key.ptr());
            }
            key = pybind11::cast("name2");
            if(PyDict_Contains(_dict, key.ptr())) {
                PyDict_DelItem(_dict, key.ptr());
            }
            key = pybind11::cast("target_temperature");
            if(PyDict_Contains(_dict, key.ptr())) {
                PyDict_DelItem(_dict, key.ptr());
            }
        }
        
        return S_OK;
    }
    catch(const std::exception &e) {
        return c_error(CERR_EXCEP, e.what());
    }

    return CERR_FAIL;
}

MxParticleType* MxParticleType_ForEngine(struct engine *e, double mass,
        double charge, const char *name, const char *name2)
{
    pybind11::dict dict;

    dict["mass"] = pybind11::cast(mass);
    dict["charge"] = pybind11::cast(charge);
    
    if(name2) {
        dict["name2"] = pybind11::cast(name2);
    }

    return MxParticleType_New(name, dict.ptr());
}

CAPI_FUNC(MxParticleType*) MxParticle_GetType()
{
    return &engine::types[0];
}

HRESULT engine_particle_base_init(PyObject *m)
{
    if(engine::max_type < 2) {
        return mx_error(E_FAIL, "must have at least space for 2 particle types");
    }

    if(engine::nr_types != 0) {
        return mx_error(E_FAIL, "engine types already set");
    }

    if((engine::types = (MxParticleData *)malloc( sizeof(MxParticleData) * engine::max_type )) == NULL ) {
        return mx_error(E_FAIL, "could not allocate types memory");
    }
    
    ::memset(engine::types, 0, sizeof(MxParticleData) * engine::max_type);

    //make an instance of the base particle type, all new instances of base
    //class mechanica.Particle will be of this type
    PyTypeObject *ob = (PyTypeObject*)&engine::types[0];
    
    Py_TYPE(ob) = &MxParticleType_Type;
    ob->tp_base = &PyBaseObject_Type;
    ob->tp_getset = particle_getsets;
    ob->tp_name =         "Particle";
    ob->tp_basicsize =     sizeof(MxPyParticle);
    ob->tp_flags =         Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    ob->tp_doc =           "Custom objects";
    ob->tp_getset =        particle_getsets;
    ob->tp_init =          (initproc)particle_init;
    ob->tp_new =           particle_new;
    ob->tp_del =           [] (PyObject *p) -> void {
        std::cout << "tp_del MxPyParticle" << std::endl;
    };
    ob->tp_finalize =      [] (PyObject *p) -> void {
        // std::cout << "tp_finalize MxPyParticle" << std::endl;
    };

    if(PyType_Ready(ob) < 0) {
        return mx_error(E_FAIL, "PyType_Ready on base particle failed");
    }

    engine::types[0].mass = 1.0;
    engine::types[0].charge = 0.0;
    engine::types[0].id = 0;

    ::strncpy(engine::types[0].name, "Particle", MxParticleType::MAX_NAME);
    ::strncpy(engine::types[0].name2, "Particle", MxParticleType::MAX_NAME);
    
    // set the singlton particle type data to the new item here.
    if (PyModule_AddObject(m, "Particle", (PyObject*)&engine::types[0]) < 0) {
        return E_FAIL;
    }

    std::cout << "added Particle to mechanica module" << std::endl;
    
    engine::nr_types = 1;
    
    return S_OK;
}
