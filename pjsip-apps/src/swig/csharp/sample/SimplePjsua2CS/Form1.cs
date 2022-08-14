using System.Drawing;
using System.Windows.Forms;

namespace SimplePjsua2CS
{
    public partial class Form1 : Form
    {
        private pjsua2xamarin.PjSample pj = new pjsua2xamarin.PjSample();
        private Panel panel;

        public Form1()
        {
            InitializeComponent();
            pj.start();
        }

        private void Form1_Load(object sender, System.EventArgs e)
        {
            panel = new Panel();
            panel.Size = new Size(350, 250);
            panel.Location = new Point(20, 20);
            Controls.Add(panel);

            pj.startPreview(panel.Handle);
            //pj.startPreview(System.IntPtr.Zero);
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            pj.stopPreview();
            pj.stop();
        }
    }
}
