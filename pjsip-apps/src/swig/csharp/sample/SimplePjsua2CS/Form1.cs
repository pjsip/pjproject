using System.Drawing;
using System.Windows.Forms;

namespace SimplePjsua2CS
{
    public partial class Form1 : Form
    {
        private pjsua2xamarin.PjSample pj = new pjsua2xamarin.PjSample();
        private Panel panel;
        private Panel panel2;

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

            // Tell SDL to use our window (panel)
            pj.startPreview(panel.Handle);

            // Let SDL creating its own window
            //pj.startPreview(System.IntPtr.Zero);
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            pj.stopPreview();
            pj.stop();
        }

        private void button1_Click(object sender, System.EventArgs e)
        {
            if (panel2 != null)
                return;

            panel2 = new Panel();
            panel2.Size = new Size(350, 250);
            panel2.Location = new Point(20, 250+20+10);
            Controls.Add(panel2);

            // Simulate incoming video window, i.e: initially SDL creates
            // its own rendering window (which by default is hidden),
            // then app may want to change the window.
            pj.updatePreviewWindow(panel2.Handle);
        }
    }
}
