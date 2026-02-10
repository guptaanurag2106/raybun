#pragma once

#include <stddef.h>

#include "common.h"
#include "rinternal.h"
#include "vec.h"

#define AABB_DELTA 0.0001f

static inline AABB aabb(const V3f a, const V3f b) {
    const float xmin = MIN(a.x, b.x);
    const float xmax = MAX(a.x, b.x);
    const float ymin = MIN(a.y, b.y);
    const float ymax = MAX(a.y, b.y);
    const float zmin = MIN(a.z, b.z);
    const float zmax = MAX(a.z, b.z);

    const float delta = AABB_DELTA;

    return (AABB){
        .xmin = (xmax - xmin) < delta ? xmin - delta / 2 : xmin,
        .xmax = (xmax - xmin) < delta ? xmax + delta / 2 : xmax,
        .ymin = (ymax - ymin) < delta ? ymin - delta / 2 : ymin,
        .ymax = (ymax - ymin) < delta ? ymax + delta / 2 : ymax,
        .zmin = (zmax - zmin) < delta ? zmin - delta / 2 : zmin,
        .zmax = (zmax - zmin) < delta ? zmax + delta / 2 : zmax,
    };
}

static inline AABB aabb_join(const AABB a, const AABB b) {
    AABB aabb = {0};

    aabb.xmin = MIN(a.xmin, b.xmin);
    aabb.xmax = MAX(a.xmax, b.xmax);
    aabb.ymin = MIN(a.ymin, b.ymin);
    aabb.ymax = MAX(a.ymax, b.ymax);
    aabb.zmin = MIN(a.zmin, b.zmin);
    aabb.zmax = MAX(a.zmax, b.zmax);

    return aabb;
}

static int g_sort_axis;
static int comparator(const void *a, const void *b) {
    const Hittable *ah = a;
    const Hittable *bh = b;
    switch (g_sort_axis) {
        case 0:
            return (ah->box.xmin > bh->box.xmin) -
                   (ah->box.xmin < bh->box.xmin);
            break;
        case 1:
            return (ah->box.ymin > bh->box.ymin) -
                   (ah->box.ymin < bh->box.ymin);
            break;
        case 2:
            return (ah->box.zmin > bh->box.zmin) -
                   (ah->box.zmin < bh->box.zmin);
            break;
        default:
            return 0;
    }
}

// construct bvh and put scene.bvh_root
static inline Hittable construct_bvh(Hittable *hittable, size_t start,
                                     size_t end) {
    if (hittable == NULL) return (Hittable){0};
    if (start == end) return (Hittable){0};

    size_t count = end - start;
    if (count == 1) {
        return hittable[start];
    }
    if (count == 2) {
        BVH_Node *node = malloc(sizeof(BVH_Node));
        node->left = hittable[start];
        node->right = hittable[end - 1];
        return make_hittable_bvh(node,
                                 aabb_join(node->left.box, node->right.box));
    }

    AABB box = {0};
    for (size_t i = start; i < end; i++) {
        box = aabb_join(box, hittable[i].box);
    }

    BVH_Node *node = malloc(sizeof(BVH_Node));
    if ((box.xmax - box.xmin) > (box.ymax - box.ymin)) {
        if ((box.xmax - box.xmin) > (box.zmax - box.zmin)) {
            g_sort_axis = 0;
        } else {
            g_sort_axis = 2;
        }
    } else {
        if ((box.ymax - box.ymin) > (box.zmax - box.zmin)) {
            g_sort_axis = 1;
        } else {
            g_sort_axis = 2;
        }
    }

    qsort(hittable + start, end - start, sizeof(Hittable), comparator);

    size_t mid = count / 2 + start;
    node->left = construct_bvh(hittable, start, mid);
    node->right = construct_bvh(hittable, mid, end);

    return make_hittable_bvh(node, box);
}
