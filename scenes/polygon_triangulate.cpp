#include "polygon_triangulate.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>

namespace demo {

namespace {

// --- Earcut (the algorithm of mapbox earcut) --------------------------------------------------------------------------

struct Node
{
    int i; // vertex of the face
    double x, y;
    Node* prev = nullptr;
    Node* next = nullptr;
    bool steiner = false;
};

// Nodes of earcut, reused from face to face: chunks are never freed or moved (nodes are linked by pointers)
class NodePool
{
public:
    void reset() { m_used = 0; }

    Node* make(int i, double x, double y)
    {
        const size_t chunk = m_used / kChunk;
        if (chunk == m_chunks.size())
            m_chunks.push_back(std::make_unique<Node[]>(kChunk));
        Node* p = &m_chunks[chunk][m_used % kChunk];
        ++m_used;
        *p = Node{i, x, y};
        return p;
    }

private:
    static constexpr size_t kChunk = 256;
    std::vector<std::unique_ptr<Node[]>> m_chunks;
    size_t m_used = 0;
};

class Earcut
{
public:
    Earcut(const std::vector<double>& xy, NodePool& pool, std::vector<Node*>& queue, std::vector<int>& out)
        : m_xy(xy)
        , m_pool(pool)
        , m_queue(queue)
        , m_out(out)
    {}

    // loops — vertex lists of the contours: the outer one and the holes
    void run(const std::vector<std::vector<int>>& loops, size_t loopCount, size_t outer)
    {
        m_pool.reset();
        Node* start = linkedList(loops[outer], true);
        if (!start || start->next == start->prev)
            return;
        std::vector<Node*>& queue = m_queue;
        queue.clear();
        for (size_t l = 0; l < loopCount; ++l) {
            if (l == outer || loops[l].size() < 3)
                continue;
            Node* list = linkedList(loops[l], false);
            if (!list)
                continue;
            if (list == list->next)
                list->steiner = true;
            queue.push_back(leftmost(list));
        }
        std::sort(queue.begin(), queue.end(), [](const Node* a, const Node* b) {
            return a->x != b->x ? a->x < b->x : a->y < b->y;
        });
        for (Node* h : queue)
            start = eliminateHole(h, start);
        earcutLinked(start, 0);
    }

private:
    Node* make(int i, double x, double y) { return m_pool.make(i, x, y); }

    Node* insert(int i, Node* last)
    {
        Node* p = make(i, m_xy[i * 2], m_xy[i * 2 + 1]);
        if (!last) {
            p->prev = p->next = p;
        } else {
            p->next = last->next;
            p->prev = last;
            last->next->prev = p;
            last->next = p;
        }
        return p;
    }

    static void remove(Node* p)
    {
        p->next->prev = p->prev;
        p->prev->next = p->next;
    }

    static double area(const Node* p, const Node* q, const Node* r)
    {
        return (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
    }

    static bool equals(const Node* a, const Node* b) { return a->x == b->x && a->y == b->y; }

    Node* linkedList(const std::vector<int>& ring, bool clockwise)
    {
        double sum = 0;
        const size_t n = ring.size();
        for (size_t k = 0; k < n; ++k) {
            const int a = ring[k], b = ring[(k + 1) % n];
            sum += (m_xy[a * 2] - m_xy[b * 2]) * (m_xy[a * 2 + 1] + m_xy[b * 2 + 1]);
        }
        Node* last = nullptr;
        if (clockwise == (sum > 0)) {
            for (int i : ring)
                last = insert(i, last);
        } else {
            for (size_t k = n; k-- > 0;)
                last = insert(ring[k], last);
        }
        if (last && equals(last, last->next)) {
            remove(last);
            last = last->next;
        }
        return last;
    }

    // Removes repeated points and points on a straight line
    static Node* filterPoints(Node* start, Node* end = nullptr)
    {
        if (!start)
            return start;
        if (!end)
            end = start;
        Node* p = start;
        bool again;
        do {
            again = false;
            if (!p->steiner && (equals(p, p->next) || area(p->prev, p, p->next) == 0)) {
                remove(p);
                p = end = p->prev;
                if (p == p->next)
                    break;
                again = true;
            } else {
                p = p->next;
            }
        } while (again || p != end);
        return end;
    }

    static bool pointInTriangle(double ax, double ay, double bx, double by, double cx, double cy, double px, double py)
    {
        return (cx - px) * (ay - py) >= (ax - px) * (cy - py) && (ax - px) * (by - py) >= (bx - px) * (ay - py)
               && (bx - px) * (cy - py) >= (cx - px) * (by - py);
    }

    static bool isEar(const Node* ear)
    {
        const Node *a = ear->prev, *b = ear, *c = ear->next;
        if (area(a, b, c) >= 0)
            return false; // reflex
        for (const Node* p = c->next; p != a; p = p->next)
            if (pointInTriangle(a->x, a->y, b->x, b->y, c->x, c->y, p->x, p->y) && area(p->prev, p, p->next) >= 0)
                return false;
        return true;
    }

    static int sign(double v) { return (v > 0) - (v < 0); }

    static bool onSegment(const Node* p, const Node* q, const Node* r)
    {
        return q->x <= std::max(p->x, r->x) && q->x >= std::min(p->x, r->x) && q->y <= std::max(p->y, r->y)
               && q->y >= std::min(p->y, r->y);
    }

    static bool intersects(const Node* p1, const Node* q1, const Node* p2, const Node* q2)
    {
        const int o1 = sign(area(p1, q1, p2)), o2 = sign(area(p1, q1, q2));
        const int o3 = sign(area(p2, q2, p1)), o4 = sign(area(p2, q2, q1));
        if (o1 != o2 && o3 != o4)
            return true;
        if (o1 == 0 && onSegment(p1, p2, q1))
            return true;
        if (o2 == 0 && onSegment(p1, q2, q1))
            return true;
        if (o3 == 0 && onSegment(p2, p1, q2))
            return true;
        if (o4 == 0 && onSegment(p2, q1, q2))
            return true;
        return false;
    }

    static bool locallyInside(const Node* a, const Node* b)
    {
        return area(a->prev, a, a->next) < 0 ? area(a, b, a->next) >= 0 && area(a, a->prev, b) >= 0
                                             : area(a, b, a->prev) < 0 || area(a, a->next, b) < 0;
    }

    Node* cureLocalIntersections(Node* start)
    {
        Node* p = start;
        do {
            Node *a = p->prev, *b = p->next->next;
            if (!equals(a, b) && intersects(a, p, p->next, b) && locallyInside(a, b) && locallyInside(b, a)) {
                emit(a, p, b);
                remove(p);
                remove(p->next);
                p = start = b;
            }
            p = p->next;
        } while (p != start);
        return filterPoints(p);
    }

    static bool intersectsPolygon(const Node* a, const Node* b)
    {
        const Node* p = a;
        do {
            if (p->i != a->i && p->next->i != a->i && p->i != b->i && p->next->i != b->i
                && intersects(p, p->next, a, b))
                return true;
            p = p->next;
        } while (p != a);
        return false;
    }

    static bool middleInside(const Node* a, const Node* b)
    {
        const Node* p = a;
        bool inside = false;
        const double px = (a->x + b->x) / 2, py = (a->y + b->y) / 2;
        do {
            if (((p->y > py) != (p->next->y > py)) && p->next->y != p->y
                && px < (p->next->x - p->x) * (py - p->y) / (p->next->y - p->y) + p->x)
                inside = !inside;
            p = p->next;
        } while (p != a);
        return inside;
    }

    static bool isValidDiagonal(const Node* a, const Node* b)
    {
        return a->next->i != b->i && a->prev->i != b->i && !intersectsPolygon(a, b)
               && ((locallyInside(a, b) && locallyInside(b, a) && middleInside(a, b)
                       && (area(a->prev, a, b->prev) != 0 || area(a, b->prev, b) != 0))
                   || (equals(a, b) && area(a->prev, a, a->next) > 0 && area(b->prev, b, b->next) > 0));
    }

    // Splits the ring by the diagonal a-b into two; returns the copy of b in the second ring
    Node* splitPolygon(Node* a, Node* b)
    {
        Node* a2 = make(a->i, a->x, a->y);
        Node* b2 = make(b->i, b->x, b->y);
        Node* an = a->next;
        Node* bp = b->prev;
        a->next = b;
        b->prev = a;
        a2->next = an;
        an->prev = a2;
        b2->next = a2;
        a2->prev = b2;
        bp->next = b2;
        b2->prev = bp;
        return b2;
    }

    void splitEarcut(Node* start)
    {
        Node* a = start;
        do {
            for (Node* b = a->next->next; b != a->prev; b = b->next) {
                if (a->i != b->i && isValidDiagonal(a, b)) {
                    Node* c = splitPolygon(a, b);
                    a = filterPoints(a, a->next);
                    c = filterPoints(c, c->next);
                    earcutLinked(a, 0);
                    earcutLinked(c, 0);
                    return;
                }
            }
            a = a->next;
        } while (a != start);
    }

    void earcutLinked(Node* ear, int pass)
    {
        if (!ear)
            return;
        Node* stop = ear;
        while (ear->prev != ear->next) {
            Node* prev = ear->prev;
            Node* next = ear->next;
            if (isEar(ear)) {
                emit(prev, ear, next);
                remove(ear);
                ear = next->next;
                stop = next->next;
                continue;
            }
            ear = next;
            if (ear == stop) {
                // no ear left: drop repeated and collinear points, then cure small self-intersections, then split
                if (pass == 0)
                    earcutLinked(filterPoints(ear), 1);
                else if (pass == 1)
                    earcutLinked(cureLocalIntersections(filterPoints(ear)), 2);
                else
                    splitEarcut(ear);
                break;
            }
        }
    }

    static Node* leftmost(Node* start)
    {
        Node *p = start, *left = start;
        do {
            if (p->x < left->x || (p->x == left->x && p->y < left->y))
                left = p;
            p = p->next;
        } while (p != start);
        return left;
    }

    static bool sectorContainsSector(const Node* m, const Node* p)
    {
        return area(m->prev, m, p->prev) < 0 && area(p->next, m, m->next) < 0;
    }

    // The outer vertex a hole is joined to: visible along a ray to the left from the hole's leftmost vertex
    static Node* findHoleBridge(Node* hole, Node* outer)
    {
        Node* p = outer;
        const double hx = hole->x, hy = hole->y;
        double qx = -std::numeric_limits<double>::infinity();
        Node* m = nullptr;
        if (equals(hole, p))
            return p;
        do {
            if (equals(hole, p->next))
                return p->next;
            if (hy <= p->y && hy >= p->next->y && p->next->y != p->y) {
                const double x = p->x + (hy - p->y) * (p->next->x - p->x) / (p->next->y - p->y);
                if (x <= hx && x > qx) {
                    qx = x;
                    m = p->x < p->next->x ? p : p->next;
                    if (x == hx)
                        return m;
                }
            }
            p = p->next;
        } while (p != outer);
        if (!m)
            return nullptr;
        const Node* stop = m;
        const double mx = m->x, my = m->y;
        double tanMin = std::numeric_limits<double>::infinity();
        p = m;
        do {
            if (hx >= p->x && p->x >= mx && hx != p->x
                && pointInTriangle(hy < my ? hx : qx, hy, mx, my, hy < my ? qx : hx, hy, p->x, p->y)) {
                const double tan = std::abs(hy - p->y) / (hx - p->x);
                if (locallyInside(p, hole)
                    && (tan < tanMin
                        || (tan == tanMin && (p->x > m->x || (p->x == m->x && sectorContainsSector(m, p)))))) {
                    m = p;
                    tanMin = tan;
                }
            }
            p = p->next;
        } while (p != stop);
        return m;
    }

    Node* eliminateHole(Node* hole, Node* outer)
    {
        Node* bridge = findHoleBridge(hole, outer);
        if (!bridge)
            return outer;
        Node* bridgeReverse = splitPolygon(bridge, hole);
        filterPoints(bridgeReverse, bridgeReverse->next);
        return filterPoints(bridge, bridge->next);
    }

    void emit(const Node* a, const Node* b, const Node* c)
    {
        m_out.push_back(a->i);
        m_out.push_back(b->i);
        m_out.push_back(c->i);
    }

    const std::vector<double>& m_xy;
    NodePool& m_pool;
    std::vector<Node*>& m_queue;
    std::vector<int>& m_out;
};

// Work buffers shared by the faces of one thread (a buffer per quad is noticeable on scenes with hundreds of thousands
// of faces)
struct Scratch
{
    std::vector<double> xy;
    std::vector<int> order;
    std::vector<signed char> kind; // 0 — an edge, 1 — a bridge there, 2 — a bridge back
    std::vector<std::vector<int>> loops;
    std::vector<int> stack;
    std::vector<int> out;
    NodePool pool;
    std::vector<Node*> queue;
};

} // namespace

void triangulatePolygon(const float* positions, int stride, int n, uint32_t base, std::vector<uint32_t>& tris)
{
    auto fan = [&]() {
        for (int v = 1; v + 1 < n; ++v) {
            tris.push_back(base);
            tris.push_back(base + v);
            tris.push_back(base + v + 1);
        }
    };
    if (n < 3)
        return;
    if (n == 3) {
        fan();
        return;
    }
    auto p = [&](int v) { return positions + static_cast<size_t>(v) * stride; };

    // Newell normal → projection onto the plane of the other two axes
    double nrm[3] = {0, 0, 0};
    for (int v = 0; v < n; ++v) {
        const float* a = p(v);
        const float* b = p((v + 1) % n);
        nrm[0] += (double(a[1]) - b[1]) * (double(a[2]) + b[2]);
        nrm[1] += (double(a[2]) - b[2]) * (double(a[0]) + b[0]);
        nrm[2] += (double(a[0]) - b[0]) * (double(a[1]) + b[1]);
    }
    const double ax = std::abs(nrm[0]), ay = std::abs(nrm[1]), az = std::abs(nrm[2]);
    const int drop = ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2);
    if (nrm[drop] == 0) {
        fan(); // degenerate face
        return;
    }
    const int ix = (drop + 1) % 3, iy = (drop + 2) % 3;
    const double sign = nrm[drop] >= 0 ? 1.0 : -1.0;

    // Convex — a fan, straight from the positions (most faces: no work buffers)
    bool convex = true;
    for (int v = 0; v < n && convex; ++v) {
        const float* a = p(v);
        const float* b = p((v + 1) % n);
        const float* c = p((v + 2) % n);
        convex = sign
                     * ((double(b[ix]) - a[ix]) * (double(c[iy]) - a[iy])
                         - (double(b[iy]) - a[iy]) * (double(c[ix]) - a[ix]))
                 >= 0;
    }
    if (convex) {
        fan();
        return;
    }

    thread_local Scratch s;
    std::vector<double>& xy = s.xy;
    xy.resize(static_cast<size_t>(n) * 2);
    for (int v = 0; v < n; ++v) {
        xy[v * 2] = p(v)[ix];
        xy[v * 2 + 1] = p(v)[iy];
    }
    auto cross2 = [&](int a, int b, int c) {
        return sign
               * ((xy[b * 2] - xy[a * 2]) * (xy[c * 2 + 1] - xy[a * 2 + 1])
                   - (xy[b * 2 + 1] - xy[a * 2 + 1]) * (xy[c * 2] - xy[a * 2]));
    };

    // Bridges: an edge walked there and back (the same positions in the opposite order). Edges are sorted by their
    // unordered pair of ends; in a run of equal edges, an edge and the next one in the opposite direction are a pair
    auto less = [&](int a, int b) {
        return xy[a * 2] != xy[b * 2] ? xy[a * 2] < xy[b * 2] : xy[a * 2 + 1] < xy[b * 2 + 1];
    };
    auto same = [&](int a, int b) { return xy[a * 2] == xy[b * 2] && xy[a * 2 + 1] == xy[b * 2 + 1]; };
    auto lo = [&](int e) { return less((e + 1) % n, e) ? (e + 1) % n : e; };
    auto hi = [&](int e) { return less((e + 1) % n, e) ? e : (e + 1) % n; };
    std::vector<signed char>& kind = s.kind;
    kind.assign(n, 0);
    bool bridged = false;
    // A bridge needs a position met twice: a small face without repeated positions (most concave faces) skips the
    // sort
    bool repeated = n > 64;
    for (int a = 0; a < n && !repeated; ++a)
        for (int b = a + 1; b < n && !repeated; ++b)
            repeated = same(a, b);
    if (repeated) {
        std::vector<int>& order = s.order;
        order.resize(n);
        for (int e = 0; e < n; ++e)
            order[e] = e;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const int la = lo(a), lb = lo(b);
            if (!same(la, lb))
                return less(la, lb);
            const int ha = hi(a), hb = hi(b);
            if (!same(ha, hb))
                return less(ha, hb);
            return a < b; // equal edges — in the order along the face
        });
        for (int k = 0; k < n;) {
            int e = k + 1;
            while (e < n && same(lo(order[e]), lo(order[k])) && same(hi(order[e]), hi(order[k])))
                ++e;
            for (int x = k; x + 1 < e; x += 2) {
                const int i = order[x], j = order[x + 1];
                if (same(i, (j + 1) % n) && same((i + 1) % n, j)) {
                    kind[std::min(i, j)] = 1;
                    kind[std::max(i, j)] = 2;
                    bridged = true;
                }
            }
            k = e;
        }
    }

    // Contours: walking the face, a bridge there opens a new contour, a bridge back returns to the previous one (the
    // vertex before a bridge back repeats the start of the contour, the vertex after it — the vertex of the parent)
    std::vector<std::vector<int>>& loops = s.loops;
    size_t loopCount = 1;
    if (loops.empty())
        loops.emplace_back();
    loops[0].assign(1, 0);
    if (bridged) {
        std::vector<int>& stack = s.stack;
        stack.assign(1, 0);
        for (int i = 0; i + 1 < n; ++i) {
            if (kind[i] == 1) {
                if (loops.size() <= loopCount)
                    loops.emplace_back();
                loops[loopCount].clear();
                stack.push_back(static_cast<int>(loopCount++));
            } else if (kind[i] == 2) {
                if (stack.size() > 1)
                    stack.pop_back();
                continue;
            }
            if (kind[i + 1] == 2)
                continue;
            loops[stack.back()].push_back(i + 1);
        }
    } else {
        for (int i = 1; i < n; ++i)
            loops[0].push_back(i);
    }

    // The outer contour — of the largest area
    auto loopArea = [&](const std::vector<int>& l) {
        double sum = 0;
        for (size_t k = 0; k < l.size(); ++k) {
            const int a = l[k], b = l[(k + 1) % l.size()];
            sum += xy[a * 2] * xy[b * 2 + 1] - xy[b * 2] * xy[a * 2 + 1];
        }
        return std::abs(sum);
    };
    size_t outer = 0;
    double best = -1;
    for (size_t l = 0; l < loopCount; ++l) {
        const double a = loopArea(loops[l]);
        if (a > best) {
            best = a;
            outer = l;
        }
    }
    std::vector<int>& out = s.out;
    out.clear();
    Earcut(xy, s.pool, s.queue, out).run(loops, loopCount, outer);
    // The winding of the face
    for (size_t k = 0; k + 2 < out.size(); k += 3) {
        int a = out[k], b = out[k + 1], c = out[k + 2];
        if (cross2(a, b, c) < 0)
            std::swap(b, c);
        tris.push_back(base + a);
        tris.push_back(base + b);
        tris.push_back(base + c);
    }
}

} // namespace demo
