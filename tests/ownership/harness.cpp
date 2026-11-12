/*
 Ownership regression harness for the SOCI plugin.

 Builds the real plugin sources (4DPlugin-SOCI.cpp, support/4DPlugin-JSON.cpp,
 C_TEXT.cpp) and the unchanged bundled 4D Plugin API (4DPluginAPI.c) against a
 synthetic Call4D boundary that models 4D's ownership rules:
   - strings, handles and pictures are individually tracked heap blocks;
   - objects and collections are reference counted and freed at zero;
   - PA_Get{ObjectProperty,CollectionElement} and command results are owned
     by the plugin; PA_Set{ObjectProperty,CollectionElement} copy the value.
 Only SQLite ":memory:" databases and synthetic data are used. The ODBC case
 names a driver that does not exist, so it fails locally before any I/O.

 Usage: harness <test-name>   (one test per process; see run.sh)
*/

#include "4DPluginAPI.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"
#include "4DPlugin-JSON.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>


namespace fake {

typedef std::u16string ustr;

struct Handle { std::vector<char> data; };
struct Obj;
struct Col { int rc = 1; std::vector<PA_Variable> items; };
struct Obj {
    int rc = 1;
    std::vector<std::pair<ustr, PA_Variable> > props;
    bool isBlob = false;
    std::vector<unsigned char> bytes;
};

static std::set<void *> liveStr, liveHandles, livePict, liveObj, liveCol;
static Obj *blobClass = nullptr, *objectClass = nullptr;
static PA_long32 failCommand = 0;

static void fail(const char *msg) {
    fprintf(stderr, "FAKE4D FATAL: %s\n", msg);
    abort();
}

static PA_Unichar *newStr(const PA_Unichar *src, PA_long32 len) {
    PA_Unichar *p = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if (len) memcpy(p, src, len * sizeof(PA_Unichar));
    p[len] = 0;
    liveStr.insert(p);
    return p;
}
static void freeStr(PA_Unichar *p) {
    if (!p) return;
    if (!liveStr.erase(p)) fprintf(stderr, "FAKE4D: string %p released twice\n", (void *)p);
    free(p); // a second free is reported by ASan
}
static Handle *newHandle(size_t n) {
    Handle *h = new Handle;
    h->data.resize(n);
    liveHandles.insert(h);
    return h;
}
static void freeHandle(Handle *h) {
    if (!h) return;
    if (!liveHandles.erase(h)) fprintf(stderr, "FAKE4D: handle %p released twice\n", (void *)h);
    delete h;
}

static void clearVar(PA_Variable *v);

static Obj *newObj() { Obj *o = new Obj; liveObj.insert(o); return o; }
static Col *newCol() { Col *c = new Col; liveCol.insert(c); return c; }
static void retainObj(Obj *o) { if (o) o->rc++; }
static void retainCol(Col *c) { if (c) c->rc++; }
static void releaseObj(Obj *o) {
    if (!o) return;
    if (--o->rc == 0) { // reading rc of a freed object is a heap-use-after-free
        for (auto &p : o->props) clearVar(&p.second);
        liveObj.erase(o);
        delete o;
    }
}
static void releaseCol(Col *c) {
    if (!c) return;
    if (--c->rc == 0) {
        for (auto &v : c->items) clearVar(&v);
        liveCol.erase(c);
        delete c;
    }
}

static PA_Variable undefinedVar() {
    PA_Variable v;
    memset(&v, 0, sizeof(v));
    v.fType = eVK_Undefined;
    return v;
}

static void clearVar(PA_Variable *v) {
    switch (v->fType) {
        case eVK_Unistring: freeStr(v->uValue.fString.fString); break;
        case eVK_Object: releaseObj((Obj *)v->uValue.fObject); break;
        case eVK_Collection: releaseCol((Col *)v->uValue.fCollection); break;
        case eVK_Blob: freeHandle((Handle *)v->uValue.fBlob.fHandle); break;
        case eVK_Picture: {
            void *pic = v->uValue.fPicture; // PA_Variable is 2-byte packed
            if (pic) {
                if (!livePict.erase(pic)) fprintf(stderr, "FAKE4D: picture released twice\n");
                delete (Handle *)pic;
            }
        } break;
        default: break;
    }
}

// value semantics used by containers: strings/blobs are copied, refs retained
static PA_Variable copyVar(const PA_Variable &in) {
    PA_Variable out = in;
    switch (in.fType) {
        case eVK_Unistring:
            out.uValue.fString.fString = newStr(in.uValue.fString.fString, in.uValue.fString.fLength);
            break;
        case eVK_Object: retainObj((Obj *)in.uValue.fObject); break;
        case eVK_Collection: retainCol((Col *)in.uValue.fCollection); break;
        case eVK_Blob: {
            Handle *src = (Handle *)in.uValue.fBlob.fHandle;
            Handle *h = newHandle(src ? src->data.size() : 0);
            if (src) h->data = src->data;
            out.uValue.fBlob.fHandle = (PA_Handle)h;
        } break;
        case eVK_Picture: {
            Handle *h = new Handle;
            livePict.insert(h);
            out.uValue.fPicture = (PA_Picture)h;
        } break;
        default: break;
    }
    return out;
}

static ustr toU(const PA_Unistring &s) { return ustr((const char16_t *)s.fString, s.fLength); }
static ustr U(const char *s) { ustr u; while (*s) u += (char16_t)(unsigned char)*s++; return u; }
static std::string A(const ustr &u) { std::string s; for (auto c : u) s += (char)c; return s; }

static PA_Variable *findProp(Obj *o, const ustr &k) {
    for (auto &p : o->props) if (p.first == k) return &p.second;
    return nullptr;
}
static void setProp(Obj *o, const ustr &k, const PA_Variable &v) {
    PA_Variable *e = findProp(o, k);
    if (e) { clearVar(e); *e = copyVar(v); }
    else o->props.push_back(std::make_pair(k, copyVar(v)));
}

static PA_Variable strVar(const ustr &s) {
    PA_Variable v = undefinedVar();
    v.fType = eVK_Unistring;
    v.uValue.fString.fLength = (PA_long32)s.size();
    v.uValue.fString.fString = newStr((const PA_Unichar *)s.data(), (PA_long32)s.size());
    return v;
}
static PA_Variable objVar(Obj *o) { PA_Variable v = undefinedVar(); v.fType = eVK_Object; v.uValue.fObject = o; return v; }
static PA_Variable colVar(Col *c) { PA_Variable v = undefinedVar(); v.fType = eVK_Collection; v.uValue.fCollection = c; return v; }
static PA_Variable longVar(PA_long32 n) { PA_Variable v = undefinedVar(); v.fType = eVK_Longint; v.uValue.fLongint = n; return v; }
static PA_Variable realVar(double d) { PA_Variable v = undefinedVar(); v.fType = eVK_Real; v.uValue.fReal = d; return v; }
static PA_Variable nullVar() { PA_Variable v = undefinedVar(); v.fType = (char)eVK_Null; return v; }

static PA_Variable executeCommand(PA_long32 id, PA_Variable **p, int n, short *err) {
    if (id == failCommand) { *err = 1; return undefinedVar(); }
    switch (id) {
        case 1471: return objVar(newObj());
        case 1472: return colVar(newCol());
        case 1230: { // OB Get type
            Obj *o = (Obj *)p[0]->uValue.fObject;
            PA_Variable *e = findProp(o, toU(p[1]->uValue.fString));
            PA_long32 t = eVK_Undefined;
            if (e) t = (e->fType == eVK_Unistring) ? eVK_Text : (unsigned char)e->fType;
            return longVar(t);
        }
        case 1719: { // OB Keys
            Obj *o = (Obj *)p[0]->uValue.fObject;
            Col *c = newCol();
            for (auto &pr : o->props) c->items.push_back(strVar(pr.first));
            return colVar(c);
        }
        case 1730: { // OB Class
            Obj *o = (Obj *)p[0]->uValue.fObject;
            Obj *k = o->isBlob ? blobClass : objectClass;
            retainObj(k);
            return objVar(k);
        }
        case 605: { // BLOB size
            Obj *o = (Obj *)p[0]->uValue.fObject;
            return longVar(o->isBlob ? (PA_long32)o->bytes.size() : 0);
        }
        case 532: { // VARIABLE TO BLOB: 37 byte header + payload, assigned into p[1]
            Obj *o = (Obj *)p[0]->uValue.fObject;
            Handle *h = newHandle(37 + o->bytes.size());
            memcpy(h->data.data() + 37, o->bytes.data(), o->bytes.size());
            if (p[1]->fType == eVK_Blob) freeHandle((Handle *)p[1]->uValue.fBlob.fHandle);
            p[1]->fType = eVK_Blob;
            p[1]->uValue.fBlob.fHandle = (PA_Handle)h;
            p[1]->uValue.fBlob.fSize = (PA_long32)h->data.size();
            return undefinedVar();
        }
        case 558: { // COPY BLOB(src; dst; srcOffset; dstOffset; len)
            Handle *src = (Handle *)p[0]->uValue.fBlob.fHandle;
            PA_long32 so = p[2]->uValue.fLongint, dof = p[3]->uValue.fLongint, len = p[4]->uValue.fLongint;
            Handle *h = newHandle(dof + len);
            memcpy(h->data.data() + dof, src->data.data() + so, len);
            freeHandle((Handle *)p[1]->uValue.fBlob.fHandle);
            p[1]->uValue.fBlob.fHandle = (PA_Handle)h;
            p[1]->uValue.fBlob.fSize = (PA_long32)h->data.size();
            return undefinedVar();
        }
        case 1217: return strVar(U("{}"));
        case 1231: { // OB Is defined
            Obj *o = (Obj *)p[0]->uValue.fObject;
            PA_Variable v = undefinedVar();
            v.fType = eVK_Boolean;
            v.uValue.fBoolean = findProp(o, toU(p[1]->uValue.fString)) != nullptr;
            return v;
        }
        case 1225: { // OB Copy
            Obj *o = (Obj *)p[0]->uValue.fObject;
            Obj *d = newObj();
            for (auto &pr : o->props) d->props.push_back(std::make_pair(pr.first, copyVar(pr.second)));
            return objVar(d);
        }
        default: {
            char m[64];
            snprintf(m, sizeof(m), "unhandled command %d", (int)id);
            fail(m);
        }
    }
    return undefinedVar();
}

static void call4d(short selector, EngineBlock *eb) {
    switch (selector) {
        case EX_GET_INFORMATION: eb->fParam2 = 0x00002000; break;
        case EX_HANDLE_MANAGER:
            switch (eb->fParam1) {
                case 1: eb->fHandle = (PA_Handle)newHandle(eb->fParam2); break;
                case 2: freeHandle((Handle *)eb->fHandle); break;
                case 3: ((Handle *)eb->fHandle)->data.resize(eb->fParam2); break;
                case 4: eb->fParam2 = ((Handle *)eb->fHandle)->data.size(); break;
                case 5: eb->fParam3 = (sLONG_PTR)((Handle *)eb->fHandle)->data.data(); break;
                default: break;
            }
            break;
        case EX_CONVERT_STRING:
            if (eb->fParam1 == 4) {
                Handle *h = (Handle *)eb->fHandle;
                if (eb->fParam3) memcpy((void *)eb->fParam3, h->data.data(), h->data.size());
                eb->fParam4 = h->data.size();
            } else if (eb->fParam1 == 5) {
                Handle *h = newHandle(eb->fParam4);
                memcpy(h->data.data(), (void *)eb->fParam3, eb->fParam4);
                eb->fHandle = (PA_Handle)h;
            } else fail("unhandled EX_CONVERT_STRING");
            break;
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *s = (const PA_Unichar *)eb->fHandle;
            PA_long32 n = 0;
            while (s[n]) n++;
            eb->fUniString1.fLength = n;
            eb->fUniString1.fString = newStr(s, n);
            eb->fUniString1.fReserved1 = eb->fUniString1.fReserved2 = 0;
        } break;
        case EX_VARIABLE_TO_STRING: { // borrowed view of the variable's own string
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if (v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString;
        } break;
        case EX_DISPOSE_UNISTRING: freeStr(eb->fUniString1.fString); break;
        case EX_CREATE_PICTURE: {
            Handle *h = new Handle;
            livePict.insert(h);
            eb->fPicture = (PA_Picture)h;
        } break;
        case EX_DISPOSE_PICTURE:
            if (eb->fPicture) {
                if (!livePict.erase(eb->fPicture)) fprintf(stderr, "FAKE4D: picture released twice\n");
                delete (Handle *)eb->fPicture;
            }
            break;
        case EX_CLEAR_VARIABLE: clearVar((PA_Variable *)eb->fPtr1); break;
        case EX_EXECUTE_COMMAND_BY_ID:
            *(PA_Variable *)eb->fPtr2 = executeCommand((PA_long32)eb->fParam1, (PA_Variable **)eb->fPtr1, (int)eb->fParam2, &eb->fError);
            break;
        case EX_GET_OBJ_VALUE: {
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if (eb->fShort == eVK_Object) {
                PA_Variable *e = findProp((Obj *)eb->fPtr1, toU(eb->fUniString1));
                *out = e ? copyVar(*e) : undefinedVar();
            } else {
                Col *c = (Col *)eb->fPtr1;
                if (eb->fLongint == -1) { *out = longVar((PA_long32)c->items.size()); break; } // "length"
                *out = (eb->fLongint >= 0 && (size_t)eb->fLongint < c->items.size()) ? copyVar(c->items[eb->fLongint]) : undefinedVar();
            }
        } break;
        case EX_SET_OBJ_VALUE: {
            PA_Variable *in = (PA_Variable *)eb->fPtr2;
            if (eb->fShort == eVK_Object) {
                setProp((Obj *)eb->fPtr1, toU(eb->fUniString1), *in);
            } else {
                Col *c = (Col *)eb->fPtr1;
                size_t i = (size_t)eb->fLongint;
                while (c->items.size() <= i) c->items.push_back(nullVar());
                clearVar(&c->items[i]);
                c->items[i] = copyVar(*in);
            }
        } break;
        default: {
            char m[64];
            snprintf(m, sizeof(m), "unhandled selector %d", (int)selector);
            fail(m);
        }
    }
}

static void init() {
    PackInitBlock ib;
    memset(&ib, 0, sizeof(ib));
    ib.fCall4Dex = call4d;
    FourDPackex(kInitPlugin, &ib, nullptr, nullptr);
    blobClass = newObj();
    { PA_Variable v = strVar(U("Blob")); setProp(blobClass, U("name"), v); clearVar(&v); }
    objectClass = newObj();
    { PA_Variable v = strVar(U("Object")); setProp(objectClass, U("name"), v); clearVar(&v); }
}

static int leaks(bool verbose) {
    releaseObj(blobClass);
    releaseObj(objectClass);
    int n = (int)(liveStr.size() + liveHandles.size() + livePict.size() + liveObj.size() + liveCol.size());
    if (verbose || n)
        printf("  live: strings=%zu handles=%zu pictures=%zu objects=%zu collections=%zu\n",
               liveStr.size(), liveHandles.size(), livePict.size(), liveObj.size(), liveCol.size());
    return n;
}

} // namespace fake

using namespace fake;

// ---- helpers to drive the SOCI command ------------------------------------

struct Call {
    PA_long32 backend = 3;
    std::string connection = ":memory:";
    Col *statements = nullptr;
    Col *bindings = nullptr;
    PA_long32 mode = 0;
    Obj *options = nullptr;
    Obj *result = nullptr;

    void run() {
        ustr cu = U(connection.c_str());
        PA_Unistring conn;
        conn.fLength = (PA_long32)cu.size();
        conn.fString = newStr((const PA_Unichar *)cu.data(), conn.fLength);
        conn.fReserved1 = conn.fReserved2 = 0;
        PA_CollectionRef s = statements, b = bindings;
        PA_ObjectRef o = options;
        void *p[6] = {&backend, &conn, &s, &b, &mode, &o};
        PA_ObjectRef r = nullptr;
        FourDPackex(1, p, nullptr, &r);
        result = (Obj *)r;
        freeStr(conn.fString);
    }
    // caller (4D) drops everything it owns
    void release() {
        releaseObj(result);
        releaseCol(statements);
        releaseCol(bindings);
        releaseObj(options);
    }
};

static Col *texts(std::vector<const char *> v) {
    Col *c = newCol();
    for (auto s : v) c->items.push_back(strVar(U(s)));
    return c;
}
static bool successOf(Obj *r) {
    PA_Variable *v = findProp(r, U("success"));
    return v && v->fType == eVK_Boolean && v->uValue.fBoolean;
}
static Col *resultsOf(Obj *r) {
    PA_Variable *v = findProp(r, U("results"));
    return (v && v->fType == eVK_Collection) ? (Col *)v->uValue.fCollection : nullptr;
}
static Obj *rowOf(Obj *r, size_t stmt, size_t row) {
    Col *res = resultsOf(r);
    if (!res || res->items.size() <= stmt || res->items[stmt].fType != eVK_Collection) return nullptr;
    Col *rows = (Col *)res->items[stmt].uValue.fCollection;
    if (rows->items.size() <= row || rows->items[row].fType != eVK_Object) return nullptr;
    return (Obj *)rows->items[row].uValue.fObject;
}
static size_t rowCount(Obj *r, size_t stmt) {
    Col *res = resultsOf(r);
    if (!res || res->items.size() <= stmt || res->items[stmt].fType != eVK_Collection) return (size_t)-1;
    return ((Col *)res->items[stmt].uValue.fCollection)->items.size();
}
static std::string textOf(Obj *o, const char *k) {
    PA_Variable *v = o ? findProp(o, U(k)) : nullptr;
    return (v && v->fType == eVK_Unistring) ? A(toU(v->uValue.fString)) : std::string("<missing>");
}
// integer columns surface as numbers or (64-bit) as decimal text
static double numOf(Obj *o, const char *k) {
    PA_Variable *v = o ? findProp(o, U(k)) : nullptr;
    if (v && v->fType == eVK_Real) return v->uValue.fReal;
    if (v && v->fType == eVK_Unistring) return atof(A(toU(v->uValue.fString)).c_str());
    return -1;
}
static void put(Obj *o, const char *k, PA_Variable v) { setProp(o, U(k), v); clearVar(&v); }

#define CHECK(c) do { if (!(c)) { printf("  check failed: %s (line %d)\n", #c, __LINE__); ok = false; } } while (0)

// ---- tests -----------------------------------------------------------------

static bool live_ddl_statement() {
    bool ok = true;
    Call c;
    c.statements = texts({"CREATE TABLE t(a INTEGER)"});
    c.run();
    CHECK(successOf(c.result));
    CHECK(rowCount(c.result, 0) == 0);
    c.release();
    return ok;
}

static bool live_numeric_rows() {
    bool ok = true;
    Call c;
    c.statements = texts({"SELECT 1 AS n UNION ALL SELECT 2"});
    c.run();
    CHECK(successOf(c.result));
    CHECK(rowCount(c.result, 0) == 2);
    CHECK(numOf(rowOf(c.result, 0, 0), "n") == 1);
    CHECK(numOf(rowOf(c.result, 0, 1), "n") == 2);
    c.release();
    return ok;
}

static bool live_text_column() {
    bool ok = true;
    Call c;
    c.statements = texts({"SELECT 'abc' AS s"});
    c.run();
    CHECK(successOf(c.result));
    CHECK(textOf(rowOf(c.result, 0, 0), "s") == "abc");
    c.release();
    return ok;
}

static bool live_text_binding() {
    bool ok = true;
    Call c;
    c.statements = texts({"CREATE TABLE t(v TEXT)", "INSERT INTO t(v) VALUES(:v)", "SELECT v FROM t"});
    c.bindings = newCol();
    c.bindings->items.push_back(nullVar());
    Obj *b = newObj();
    put(b, "v", strVar(U("hello")));
    c.bindings->items.push_back(objVar(b));
    c.run();
    CHECK(successOf(c.result));
    CHECK(textOf(rowOf(c.result, 2, 0), "v") == "hello");
    CHECK(b->rc == 1);
    c.release();
    return ok;
}

static bool live_non_object_binding() {
    bool ok = true;
    Call c;
    c.statements = texts({"SELECT 1 AS n"});
    c.bindings = texts({"not-an-object"});
    c.run();
    CHECK(successOf(c.result));
    c.release();
    return ok;
}

static bool live_plain_object_binding() {
    bool ok = true;
    Call c;
    c.statements = texts({"CREATE TABLE t(v BLOB)", "INSERT INTO t(v) VALUES(:v)", "SELECT length(v) AS n FROM t"});
    c.bindings = newCol();
    c.bindings->items.push_back(nullVar());
    Obj *b = newObj();
    Obj *plain = newObj();
    put(b, "v", objVar(plain)); // ownership of plain passes to b
    c.bindings->items.push_back(objVar(b));
    c.run();
    CHECK(successOf(c.result));
    CHECK(b->rc == 1);
    CHECK(plain->rc == 1);
    c.release();
    return ok;
}

static Obj *makeBlob(std::vector<unsigned char> bytes) {
    Obj *o = newObj();
    o->isBlob = true;
    o->bytes = bytes;
    return o;
}

static bool live_blob_binding() {
    bool ok = true;
    Call c;
    c.statements = texts({"CREATE TABLE t(v BLOB)", "INSERT INTO t(v) VALUES(:v)", "SELECT hex(v) AS h FROM t"});
    c.bindings = newCol();
    c.bindings->items.push_back(nullVar());
    Obj *b = newObj();
    Obj *blob = makeBlob({1, 2, 3});
    put(b, "v", objVar(blob)); // ownership of blob passes to b
    c.bindings->items.push_back(objVar(b));
    c.run();
    CHECK(successOf(c.result));
    CHECK(textOf(rowOf(c.result, 2, 0), "h") == "010203");
    CHECK(blob->rc == 1);
    c.release();
    return ok;
}

static bool live_error_path() {
    bool ok = true;
    Call c;
    c.statements = texts({"SELEC nonsense"});
    c.run();
    CHECK(!successOf(c.result));
    CHECK(textOf(c.result, "errorMessage") != "<missing>");
    c.release();
    return ok;
}

static bool live_odbc_options() {
    bool ok = true;
    Call c;
    c.backend = 0;
    c.connection = "DRIVER={soci-ownership-harness-nonexistent-driver}";
    c.statements = texts({});
    c.options = newObj();
    put(c.options, "odbc_option_driver_complete", strVar(U("0")));
    c.run();
    CHECK(!successOf(c.result));
    printf("  errorMessage: %s\n", textOf(c.result, "errorMessage").c_str());
    CHECK(c.options->rc == 1);
    c.release();
    return ok;
}

// controls: no defect expected on either tree
static bool control_null_statements() {
    bool ok = true;
    Call c;
    c.run();
    CHECK(successOf(c.result));
    CHECK(resultsOf(c.result) && resultsOf(c.result)->items.empty());
    c.release();
    return ok;
}

static bool control_non_text_statement() {
    bool ok = true;
    Call c;
    c.statements = newCol();
    c.statements->items.push_back(realVar(42));
    c.run();
    CHECK(successOf(c.result));
    c.release();
    return ok;
}

static bool control_ob_stringify() {
    bool ok = true;
    Obj *o = newObj();
    CUTF8String s;
    ob_stringify(o, &s);
    CHECK(s == (const uint8_t *)"{}");
    releaseObj(o);
    return ok;
}

// dormant helpers: built, no caller in this plugin
static bool dormant_ob_set_s_wide() {
    bool ok = true;
    Obj *o = newObj();
    ob_set_s(o, L"k", "v");
    CHECK(textOf(o, "k") == "v");
    releaseObj(o);
    return ok;
}

static bool dormant_ob_set_a() {
    bool ok = true;
    Obj *o = newObj();
    ob_set_a(o, L"a", L"x");
    CUTF16String u;
    u.push_back('y');
    ob_set_a(o, L"b", &u);
    CHECK(textOf(o, "a") == "x");
    CHECK(textOf(o, "b") == "y");
    releaseObj(o);
    return ok;
}

static bool dormant_ob_set_p() {
    bool ok = true;
    Obj *o = newObj();
    ob_set_p(o, L"p", PA_CreatePicture(nullptr, 0)); // ownership passes to ob_set_p
    PA_Variable *v = findProp(o, U("p"));
    CHECK(v && v->fType == eVK_Picture);
    releaseObj(o);
    return ok;
}

static bool dormant_ob_get_on_text() {
    bool ok = true;
    Obj *o = newObj();
    put(o, "k", strVar(U("text")));
    CHECK(ob_get_b(o, L"k") == false);
    CHECK(ob_get_n(o, L"k") == 0.0);
    releaseObj(o);
    return ok;
}

static bool dormant_ob_get_c() {
    bool ok = true;
    Obj *o = newObj();
    Col *c = newCol();
    put(o, "c", colVar(c)); // ownership of c passes to o
    PA_CollectionRef got = ob_get_c(o, L"c");
    CHECK(got == (PA_CollectionRef)c);
    CHECK(c->rc == 1);
    releaseObj(o);
    return ok;
}

struct Test { const char *name; bool (*fn)(); };
static const Test tests[] = {
    {"live_ddl_statement", live_ddl_statement},
    {"live_numeric_rows", live_numeric_rows},
    {"live_text_column", live_text_column},
    {"live_text_binding", live_text_binding},
    {"live_non_object_binding", live_non_object_binding},
    {"live_plain_object_binding", live_plain_object_binding},
    {"live_blob_binding", live_blob_binding},
    {"live_error_path", live_error_path},
    {"live_odbc_options", live_odbc_options},
    {"dormant_ob_set_s_wide", dormant_ob_set_s_wide},
    {"dormant_ob_set_a", dormant_ob_set_a},
    {"dormant_ob_set_p", dormant_ob_set_p},
    {"dormant_ob_get_on_text", dormant_ob_get_on_text},
    {"dormant_ob_get_c", dormant_ob_get_c},
    {"control_null_statements", control_null_statements},
    {"control_non_text_statement", control_non_text_statement},
    {"control_ob_stringify", control_ob_stringify},
};

int main(int argc, char **argv) {
    if (argc < 2) {
        for (auto &t : tests) printf("%s\n", t.name);
        return 0;
    }
    for (auto &t : tests) {
        if (strcmp(t.name, argv[1]) == 0) {
            init();
            bool ok = t.fn();
            int n = leaks(false);
            if (n) { printf("  leaked %d tracked block(s)\n", n); ok = false; }
            printf("%s %s\n", ok ? "PASS" : "FAIL", t.name);
            return ok ? 0 : 1;
        }
    }
    fprintf(stderr, "unknown test %s\n", argv[1]);
    return 2;
}
