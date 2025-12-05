#include <math.h>

#include "common.h"
#include "vec.h"

// always scatter, attenuate, though with prob (1-reflectance R) we can just
// scatter
bool lambertian_scatter(const Material *mat, const HitRecord *rec,
                        const Ray *ray_in, Colour *attenuation, Ray *ray_out) {
    UNUSED(ray_in);
    if (mat->type != MAT_LAMBERTIAN) return false;

    V3f scatter_dir = v3f_add(rec->normal, v3f_random_unit());
    if (v3f_near_zero(scatter_dir)) {
        scatter_dir = rec->normal;
    }
    *ray_out = (Ray){rec->point, scatter_dir, v3f_slength(scatter_dir),
                     v3f_inv(scatter_dir)};
    *attenuation = mat->properties.lambertian.albedo;
    return true;
}

bool metal_scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
                   Colour *attenuation, Ray *ray_out) {
    if (mat->type != MAT_METAL) return false;

    V3f reflected_dir = v3f_reflect(ray_in->direction, rec->normal);
    reflected_dir =
        v3f_add(v3f_normalize(reflected_dir),
                v3f_mulf(v3f_random_unit(), mat->properties.metal.fuzz));
    *ray_out = (Ray){rec->point, reflected_dir, v3f_slength(reflected_dir),
                     v3f_inv(reflected_dir)};
    *attenuation = mat->properties.metal.albedo;
    return v3f_dot(reflected_dir, rec->normal) > 0;
}

static float reflectance(float cosine, float refraction_index) {
    // Use Schlick's approximation for reflectance.
    float r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1 - r0) * powf((1 - cosine), 5);
}

bool dielectric_scatter(const Material *mat, const HitRecord *rec,
                        const Ray *ray_in, Colour *attenuation, Ray *ray_out) {
    if (mat->type != MAT_DIELECTRIC) return false;
    *attenuation = (Colour){1, 1, 1};
    float ri = rec->front_face ? (1.0 / mat->properties.dielectric.etai_eta)
                               : mat->properties.dielectric.etai_eta;

    V3f norm_direction = v3f_normalize(ray_in->direction);
    float cost = MIN(-1 * v3f_dot(norm_direction, rec->normal), 1.0);
    float sint = sqrtf(1.0 - cost * cost);

    bool can_refract = ri * sint <= 1.0;
    V3f direction;
    if (can_refract && reflectance(cost, ri) < randf()) {
        direction = v3f_refract(norm_direction, rec->normal, ri);
    } else {
        direction = v3f_reflect(norm_direction, rec->normal);
    }
    *ray_out = (Ray){rec->point, direction, v3f_slength(direction),
                     v3f_inv(direction)};
    return true;
}

bool scatter(const Material *mat, const HitRecord *rec, const Ray *ray_in,
             Colour *attenuation, Ray *ray_out) {
    if (mat->type == MAT_LAMBERTIAN)
        return lambertian_scatter(mat, rec, ray_in, attenuation, ray_out);
    if (mat->type == MAT_METAL)
        return metal_scatter(mat, rec, ray_in, attenuation, ray_out);
    if (mat->type == MAT_DIELECTRIC)
        return dielectric_scatter(mat, rec, ray_in, attenuation, ray_out);
    if (mat->type == MAT_EMISSIVE) return false;

    return false;
}
