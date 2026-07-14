in vec4 C;
in vec3 posViewSpace;
in vec3 normalViewSpace;

uniform mat4 MVP;
uniform mat4 MV;
uniform mat4 MVit;
uniform vec3 kd;

uniform vec4 lightPosition;

out vec4 color;

void main()
{
  const vec3 ka = vec3(0.05f, 0.05f, 0.05f);
  const vec3 ks = vec3(1.0f, 1.0f, 1.0f);
  const float shininess = 50.0f;

  const vec3 la = vec3(0.9f, 0.9f, 0.9f);
  const vec3 ld = vec3(0.9f, 0.9f, 0.9f);
  const vec3 ls = vec3(0.9f, 0.9f, 0.9f);

  vec3 normalViewSpace = normalize(normalViewSpace); // re-normalize normal because component-wise interpolation causes non normalized normals
  vec3 L = normalize(lightPosition.xyz - posViewSpace); // from surface to lightPos

  // ambient color
  vec3 ambient = ka * la;

  // diffuse color
  float d = max(0.0f, dot(normalViewSpace, L));
  vec3 diffuse = d * kd * ld;

  float s = 0.0f;
  if(d > 0.0f)
  {
    vec3 V = normalize(-posViewSpace); // camera is placed in origin in view space, view vector == -posViewSpace
    vec3 R =  reflect(-L, normalViewSpace); // reflect expects L pointing to surface
    s = pow(max(0.0f, dot(V, R)), shininess);
  }

  vec3 specular = s * ks * ls;

  color = vec4(ambient + diffuse + specular, 1.0f);
}
