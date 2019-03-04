using System.Net.Http;
using System.Threading;
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
        private static int test = 0;

        public override async Task ProcessRequestAsync(HttpContext context)
        {
            await HttpClient.GetAsync("https://www.bing.com?k=" + Interlocked.Increment(ref test));

            context.Response.ContentType = "text/plain";
            context.Response.Write("Hello World");
        }
    }
}
