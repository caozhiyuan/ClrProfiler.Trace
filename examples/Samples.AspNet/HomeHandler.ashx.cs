using System.Net.Http;
using System.Threading.Tasks;
using System.Web;

namespace Samples.AspNet
{
    /// <summary>
    /// HomeHandler 的摘要说明
    /// </summary>
    public class HomeHandler : HttpTaskAsyncHandler
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        public override async Task ProcessRequestAsync(HttpContext context)
        {
            await HttpClient.GetAsync("https://www.bing.com");

            context.Response.ContentType = "text/plain";
            context.Response.Write("Hello World");
        }
    }
}
