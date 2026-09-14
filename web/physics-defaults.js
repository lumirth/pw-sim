// SI units. Nintendo specifies 20.6 g with a CR2032, 21.9 g with the clip.
// Cord properties are estimates for light, 1 mm cotton twine, not a measured sample.
export const defaults={
 environment:'outdoor',tether:'pendulum',length:.10,cordDiameter:.001,twistStiffness:3,bendDamping:200,
 mass:.0219,diameter:.048,thickness:.0147,clip:true,
 damping:0,response:1,grip:'point',lockFacing:false,
 wind:0,direction:0,gust:.15,fanHeight:0,oscillate:false,sweep:30,oscillationHz:.15,
 cadence:2,mode:'still',gravity:9.80665,friction:.55,restitution:.08,ground:-.10,
};

// Surface coefficients are material estimates; controls remain adjustable.
export const surfaces={
 outdoor:{ground:-.10,friction:.55,restitution:.08},
 desk:{ground:-.059,friction:.35,restitution:.16},
 studio:{ground:-.10,friction:.5,restitution:.10},
};
