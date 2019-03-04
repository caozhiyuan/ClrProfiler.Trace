using System.Net.Http;
using System.Threading.Tasks;
using System.Web.Http;

namespace Samples.AspNet.Controllers
{
    public class HomeController : ApiController
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        [HttpGet]
        public async Task<IHttpActionResult> Index()
        {
            await HttpClient.GetAsync("https://www.bing.com");
            return Json("1");
        }
    }
}
