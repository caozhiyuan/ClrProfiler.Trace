using System;
using System.Web.Http;

namespace Samples.AspNet
{
    public class Global : System.Web.HttpApplication
    {

        protected void Application_Start(object sender, EventArgs e)
        {
            GlobalConfiguration.Configure(WebApiConfig.Register);
        }

        private static class WebApiConfig
        {
            public static void Register(HttpConfiguration config)
            {
                // Web API 配置和服务	

                // Web API 路由	
                config.MapHttpAttributeRoutes();

                config.Routes.MapHttpRoute(
                    name: "DefaultApi",
                    routeTemplate: "api/{controller}/{id}",
                    defaults: new { id = RouteParameter.Optional }
                );
            }
        }
    }
}
